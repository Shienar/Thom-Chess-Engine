#include "compatibility.h"
#include "debug.h"
#include "pyrrhic/tbprobe.h"
#include "board/bitboard.h"
#include "board/magic.h"
#include "transposition/transpositiontable.h"
#include "analyze/cuckoo.h"
#include "analyze/hce/hce.h"
#include "analyze/hce/tuner.h"
#include "analyze/nnue/neuralnet.h"
#include "analyze/search.h"
#include "binpack/viri_binpack.h"
#include "binpack/generate.h"
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <omp.h>

#ifdef TRAIN
#include "train/train.h"
#endif


#ifdef SPSA

#define SET_SPA_OPTION_INT(name)																													\
	else if (strcmp(str, #name) == 0) {																												\
		if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) { \
			sscanf(str, "%d", &name);																												\
			ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);																				\
		}																																			\
		break;																																		\
	}

#define SET_SPA_OPTION_LONGLONG(name)																												\
	else if (strcmp(str, #name) == 0) {																												\
		if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) { \
			sscanf(str, "%lld", &name);																												\
			ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);																				\
		}																																			\
		break;																																		\
	}

#define SET_SPA_OPTION_FLOAT(name)																													\
	else if (strcmp(str, #name) == 0) {																												\
		if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {	\
			int temp;																																\
			sscanf(str, "%d", &temp);																												\
			name = temp / 1000.0;																													\
			ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);																				\
		}																																			\
		break;																																		\
	}

#endif

void ready_up(int *is_path_dirty, int *is_ready, char *SyzygyPath, thread_context * context);

int main(int argc, char **argv)
{
	omp_set_num_threads(thread_count);
	srand(time(NULL));

	char buffer[4096] = {'\0'};
	const char *delim = " \t\r\n";
	char *str = NULL;
	char *strtok_ptr = NULL;

	int quit = 0;
	int is_ready = 0;

	char SyzygyPath[1024] = {'\0'};
	int is_path_dirty = 1; //Has syzygy been initialized with the current path?

	char debug_log_path[1024] = {'\0'};

	thread_context *context = calloc(1, sizeof(thread_context));
	bitboard *board = &context->board_stack[0];

	context->max_depth = MAX_PLY;
	context->hard_max_nodes = INT32_MAX;
	context->soft_max_nodes = INT32_MAX;
	context->abort_flag = &abort_flag;

	THREADTYPE calculate_thread = THREAD_INIT;

	while (!quit) {
		memset(buffer, 0, 4096 * sizeof(char));
		while (fgets(buffer, 4096, stdin) == NULL);
		str = _strtok(buffer, delim, &strtok_ptr);
		while (str != NULL) {
			if (strcmp(str, "uci") == 0) {
				printf("id name Thom v1.1.0\n");
				printf("id author Grant Vizaniaris\n");
				printf("option name Hash type spin default 256 min 1 max 4096\n");
				printf("option name Threads type spin default 1 min 1 max 64\n");
				printf("option name Ponder type check default false\n");
				printf("option name UseNNUE type check default true\n");
				printf("option name LogFilePath type string default <empty>\n");
				printf("option name SyzygyPath type string default <empty>\n");
				printf("option name SyzygyProbeLimit type spin default 5 min 3 max 7\n"); //n-man syzygy tablebase.
				printf("option name SyzygyProbeDepth type spin default 6 min 5 max 32\n"); //Probe syzygy at non-root if at least n depth remaining in search.
				#ifdef SPSA
				printf("option name initial_aspiration_margin type spin default %d min 10 max 100\n", initial_aspiration_margin);
				printf("option name aspiration_margin_mult_factor type spin default %d min 1200 max 3000\n", (int) (aspiration_margin_mult_factor * 1000));

				printf("option name razoring_a type spin default %d min 25 max 250\n", razoring_a);
				printf("option name razoring_b type spin default %d min 25 max 250\n", razoring_b);

				printf("option name futility_margin type spin default %d min 25 max 250\n", futility_margin);
				printf("option name futility_depth_margin type spin default %d min 25 max 250\n", futility_depth_margin);


				printf("option name reverse_futility_margin type spin default %d min 100 max 400\n", reverse_futility_margin);
				printf("option name reverse_futility_margin_improving type spin default %d min 50 max 200\n", reverse_futility_margin_improving);

				//History values most closely affect branching factor and nodecount. Intead of SPSA tuning them with
				//weather factory, I modify them with a scipy.minimize() script to minimize nodecount at a fixed depth
				//across a perft suite, then verify the results with a SPRT, which is a much faster tuning method.
				printf("option name history_bonus_scale type spin default %d min 1 max 600\n", history_bonus_scale);
				printf("option name history_bonus_offset type spin default %d min 1 max 600\n", history_bonus_offset);
				printf("option name history_penalty_scale type spin default %d min 1 max 600\n", history_penalty_scale);
				printf("option name history_penalty_offset type spin default %d min 1 max 600\n", history_penalty_offset);

				printf("option name capture_history_bonus_scale type spin default %d min 1 max 600\n", capture_history_bonus_scale);
				printf("option name capture_history_bonus_offset type spin default %d min 1 max 600\n", capture_history_bonus_offset);
				printf("option name capture_history_penalty_scale type spin default %d min 1 max 600\n", capture_history_penalty_scale);
				printf("option name capture_history_penalty_offset type spin default %d min 1 max 600\n", capture_history_penalty_offset);

				printf("option name lmr_a type spin default %d min 100 max 2000\n", (int) (lmr_a * 1000));
				printf("option name lmr_b type spin default %d min 1000 max 5000\n", (int) (lmr_b * 1000));

				printf("option name lmp_a type spin default %d min 500 max 7000\n", (int) (lmp_a * 1000));
				printf("option name lmp_b type spin default %d min 500 max 7000\n", (int) (lmp_b * 1000));
				printf("option name lmp_improving_a type spin default %d min 500 max 7000\n", (int) (lmp_improving_a * 1000));
				printf("option name lmp_improving_b type spin default %d min 500 max 7000\n", (int) (lmp_improving_b * 1000));

				printf("option name stable_eval_margin type spin default %d min 0 max 75\n", stable_eval_margin);
				#endif
				printf("uciok\n");
				fflush(stdout);
				break;
			} else if (strcmp(str, "ucinewgame") == 0) {
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
				if (is_searching) {
					abort_flag = 1;
					THREAD_WAIT(calculate_thread);
				}
				clear_tt(transposition_table);
				memset(context->pawn_corrhist, 0, sizeof(context->pawn_corrhist));
				memset(context->nonpawn_corrhist, 0, sizeof(context->nonpawn_corrhist));
				break;
			} else if (strcmp(str, "setoption") == 0) {
				if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "name") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
					if (strcmp(str, "Hash") == 0) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							uint64_t byte_size;

							sscanf(str, "%" PRIu64 "", &byte_size);
							tt_bytes = (byte_size * 1024 * 1024);
							destroy_tt_table(transposition_table);
							transposition_table = create_tt_table();
							context->tt = transposition_table;
						}
						break;
					} else if (strcmp(str, "Threads") == 0) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							if (is_searching) {
								DEBUG_ERROR("Cannot change thread count while calculating.");
								break;
							}
							sscanf(str, "%d", &thread_count);
							thread_count = _min(MAX_THREADS, _max(MIN_THREADS, thread_count));
							omp_set_num_threads(thread_count);
						}

						break;
					} else if (strcmp(str, "Ponder") == 0) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							if (strcmp(str, "true") == 0)
								enable_ponder = 1;
							else
								enable_ponder = 0;
						}
						break;
					} else if (strcmp(str, "UseNNUE") == 0 && is_network_loaded) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							if (strcmp(str, "true") == 0)
								use_nnue = 1;
							else
								use_nnue = 0;
						}
						ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
						break;
					} else if (strcmp(str, "Clear") == 0) {
						str = _strtok(NULL, delim, &strtok_ptr);
						if (str && strcmp(str, "Hash") == 0)
							clear_tt(transposition_table);
						break;
					} else if (strcmp(str, "LogFilePath") == 0) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							if (strcmp(str, "<empty>") == 0)
								debug_log_path[0] = '\0';
							else {
								strncpy(debug_log_path, str, 1023);
								debug_log_path[1023] = '\0';
							}
							if (print_debug_messages) {
								disable_debug_messages();
								enable_debug_messages();
							}
						}
						break;
					} else if (strcmp(str, "SyzygyPath") == 0) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							if (strcmp(str, "<empty>") == 0)
								debug_log_path[0] = '\0';
							else {
								strncpy(SyzygyPath, str, 1023);
								SyzygyPath[1023] = '\0';
							}
							is_path_dirty = 1;
							if (is_ready)
								ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
						}
						break;
					} else if (strcmp(str, "SyzygyProbeLimit") == 0) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							sscanf(str, "%d", &syzygyProbeLimit);
							syzygyProbeLimit = _min(MAX_PROBE_LIMIT, _max(MIN_PROBE_LIMIT, syzygyProbeLimit));
						}
						break;
					} else if (strcmp(str, "SyzygyProbeDepth") == 0) {
						if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "value") == 0  && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							if (str)
								sscanf(str, "%d", &syzygyProbeDepth);
							syzygyProbeDepth = _min(MAX_PROBE_DEPTH, _max(MIN_PROBE_DEPTH, syzygyProbeDepth));
						}
						break;
					}
					#ifdef SPSA
					SET_SPA_OPTION_INT(initial_aspiration_margin)
					SET_SPA_OPTION_FLOAT(aspiration_margin_mult_factor)
					SET_SPA_OPTION_INT(razoring_a)
					SET_SPA_OPTION_INT(razoring_b)
					SET_SPA_OPTION_INT(reverse_futility_margin)
					SET_SPA_OPTION_INT(reverse_futility_margin_improving)
					SET_SPA_OPTION_INT(futility_margin)
					SET_SPA_OPTION_INT(futility_depth_margin)
					SET_SPA_OPTION_INT(history_bonus_scale)
					SET_SPA_OPTION_INT(history_bonus_offset)
					SET_SPA_OPTION_INT(history_penalty_scale)
					SET_SPA_OPTION_INT(history_penalty_offset)
					SET_SPA_OPTION_INT(capture_history_bonus_scale)
					SET_SPA_OPTION_INT(capture_history_bonus_offset)
					SET_SPA_OPTION_INT(capture_history_penalty_scale)
					SET_SPA_OPTION_INT(capture_history_penalty_offset)
					SET_SPA_OPTION_INT(stable_eval_margin)
					SET_SPA_OPTION_FLOAT(lmr_a)
					SET_SPA_OPTION_FLOAT(lmr_b)
					SET_SPA_OPTION_FLOAT(lmp_a)
					SET_SPA_OPTION_FLOAT(lmp_b)
					SET_SPA_OPTION_FLOAT(lmp_improving_a)
					SET_SPA_OPTION_FLOAT(lmp_improving_b)
					#endif
					break;
				}
			} else if (strcmp(str, "isready") == 0) {
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
				printf("readyok\n");
				fflush(stdout);
			} else if (strcmp(str, "debug") == 0) {
				if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
					if (strcmp(str, "on") == 0)
						enable_debug_messages();
					else if (strcmp(str, "off") == 0)
						disable_debug_messages();
				}
				break;
			} else if (strcmp(str, "position") == 0) {
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
				if (is_searching) {
					abort_flag = 1;
					THREAD_WAIT(calculate_thread);
				}

				if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
					if (strcmp(str, "fen") == 0) {
						char fen[128] = {'\0'};
						short insert_index = 0;
						short length = 0;

						for (int i = 0; i < 6; i++) {
							if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
								break;

							length = strlen(str);
							strncpy(&fen[insert_index], str, 127 - insert_index);
							fen[127] = '\0';
							insert_index += length;
							if (i < 5)
								fen[insert_index++] = ' ';
						}

						//Invalid fen
						if (!str)
							break;

						load_fen_string_to_board(board, fen, &context->repetitions);
					} else if (strcmp(str, "startpos") == 0)
						load_fen_string_to_board(board, STARTPOS_FEN, &context->repetitions);

					if ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && strcmp(str, "moves") == 0) {
						move m;
						bitboard temp_board = *board;

						while ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
							m = get_struct_from_string(board, str);
							if (move_from_struct(board, board, m, &context->repetitions)) {
								//Recover from illegal move
								memcpy(board, &temp_board, sizeof(bitboard));
								break;
							}

							memcpy(&temp_board, board, sizeof(bitboard));
						}
					}
				}
				break;
			} else if (strcmp(str, "go") == 0) {
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
				is_ponder = 0;

				if (is_searching) {
					DEBUG_INFO("Aborting search in progress.");
					abort_flag = 1;
					BROADCAST_COND_VAR(ponder_cond_var);
					THREAD_WAIT(calculate_thread);
				}
				is_searching = 1;

				int time_left = INT32_MAX;
				int increment = 0;
				int fixed_move_time = 0;
				int is_infinite = 0;

				context->hard_max_nodes = INT32_MAX;
				context->soft_max_nodes = INT32_MAX;
				memset(context->searched_moves, 0, MAX_REQUIRED_MOVES * sizeof(move));

				short searched_move_count = 0;

				while ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
					if (strcmp(str, "infinite") == 0)
						is_infinite = 1;
					else if (strcmp(str, "ponder") == 0)
						is_ponder = 1;
					else if (strcmp(str, "wtime") == 0 && ISWHITE(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
						sscanf(str, "%d", &time_left);
					else if (strcmp(str, "btime") == 0 && ISBLACK(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
						sscanf(str, "%d", &time_left);
					else if (strcmp(str, "winc") == 0 && ISWHITE(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
						sscanf(str, "%d", &increment);
					else if (strcmp(str, "binc") == 0 && ISBLACK(board->turn) && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
						sscanf(str, "%d", &increment);
					else if (strcmp(str, "depth") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
						sscanf(str, "%d", &context->max_depth);
					else if (strcmp(str, "nodes") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
						sscanf(str, "%d", &context->hard_max_nodes);
					else if (strcmp(str, "movetime") == 0 && (str = _strtok(NULL, delim, &strtok_ptr)) != NULL)
						sscanf(str, "%d", &fixed_move_time);
					else if (strcmp(str, "searchmoves") == 0) //Assume this is the final command in list.
						while ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL && searched_move_count < MAX_REQUIRED_MOVES)
							context->searched_moves[searched_move_count++] = get_struct_from_string(board, str);
				}

				context->max_depth = _min(MAX_PLY, _max(1, context->max_depth));

				//Finished parsing command modifers, setup & launch thread.
				context->start_time = clock();
				if (fixed_move_time)
					context->soft_end_time = context->hard_end_time = context->start_time + (fixed_move_time * CLOCKS_PER_SEC) / 1000;
				else if (is_infinite) {
					context->soft_end_time = context->hard_end_time = INT_MAX;
					context->max_depth = MAX_PLY;
				} else {
					int uci_overhead = _min(25, time_left / 2);
					clock_t soft_end_time = time_left / 20 + increment / 2 - uci_overhead;
					clock_t hard_end_time = time_left / 20 + increment / 2 - uci_overhead;

					context->soft_end_time = (context->start_time + (soft_end_time * CLOCKS_PER_SEC) / 1000);
					context->hard_end_time = (context->start_time + (hard_end_time * CLOCKS_PER_SEC) / 1000);
				}

				THREAD_START(calculate_thread, calculate_best_move, context);
				break;
			} else if (strcmp(str, "ponderhit") == 0) {
				if (is_searching) {
					is_ponder = 0;
					BROADCAST_COND_VAR(ponder_cond_var);
					THREAD_WAIT(calculate_thread);
				} else {
					is_ponder = 0;
					THREAD_START(calculate_thread, calculate_best_move, context);
				}
			} else if (strcmp(str, "stop") == 0) {
				//Reap the thread, which will print out its results as it terminates.
				if (is_searching) {
					is_ponder = 0;
					BROADCAST_COND_VAR(ponder_cond_var);
					abort_flag = 1;
					THREAD_WAIT(calculate_thread);
				}

			} else if (strcmp(str, "perft") == 0) {
				//Format: "perft <depth> [--verbose | -v] [--iterator | -iter]"
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);

				int pdepth = 1;
				int verbose = 0;
				int use_iterator = 0;

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;
				sscanf(str, "%d", &pdepth);

				while ((str = _strtok(NULL, delim, &strtok_ptr)) != NULL) {
					if (strcmp(str, "--verbose") == 0 || strcmp(str, "-v") == 0)
						verbose = 1;
					else if (strcmp(str, "--iterator") == 0 || strcmp(str, "-iter") == 0)
						use_iterator = 1;
				}

				clock_t start_time = clock();
				uint64_t result;

				if (!use_iterator)
					result = perft(board, pdepth, verbose);
				else
					result = iterator_perft(context, pdepth, 0, verbose);

				clock_t duration = clock() - start_time;
				double seconds = ((double) duration / CLOCKS_PER_SEC);
				double nps = result / seconds;

				printf("Searched through %" PRId64 " nodes in %f seconds at %f nps.\n", result, seconds, nps);
				fflush(stdout);
				break;
			} else if (strcmp(str, "print") == 0) {
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
				board_print(board);
				break;
			} else if (strcmp(str, "eval") == 0) {
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);
				if (use_nnue)
					update_accumulator_from_table(board, &context->accumulator_stack[0], context->refresh_table);
				printf("%d\n", use_nnue ? forward_propagate(context, 0) : hce_eval(board));
				break;
			} else if (strcmp(str, "tune") == 0) {
				//Format: 'tune <double forced_k (0 for auto)> <uint64_t epochs> <double max_lr> <double min_lr> "<input_path>" "<output_path>"'

				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);

				char input_path[256] = {'\0'};
				char output_path[256] = {'\0'};
				double forced_k;
				uint64_t epochs;
				double max_lr;
				double min_lr;

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%lf", &forced_k);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%" PRId64" ", &epochs);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%lf", &max_lr);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%lf", &min_lr);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "\"%[^\"]", input_path);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "\"%[^\"]", output_path);
				tune(input_path, output_path, forced_k, epochs, max_lr, min_lr);


				break;
			} else if (strcmp(str, "generate") == 0) {
				//Format: 'generate "<outputFilePath>"'
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				generate(str);
				break;
			} else if (strcmp(str, "binpackinfo") == 0) {
				//Format: 'binpackinfo "<binpackFilePath>"
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				binpack_print_info(str);
				break;
			} else if (strcmp(str, "cleanbinpack") == 0) {
				//Format: 'cleanbinpack "<cleanbinpack>"
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				binpack_clean(str);
				break;
			}
			#ifdef TRAIN
			else if (strcmp(str, "train") == 0) {
				//Format: "train <epochs> <min_lr> <max_lr> <binpack file path> <kernel file path>"
				ready_up(&is_path_dirty, &is_ready, SyzygyPath, context);

				int epoch_count;
				float min_lr;
				float max_lr;
				char binpack_path[256] = {'\0'};
				char kernel_path[256] = {'\0'};

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%d", &epoch_count);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%f", &min_lr);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%f", &max_lr);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%s", binpack_path);

				if ((str = _strtok(NULL, delim, &strtok_ptr)) == NULL)
					break;

				sscanf(str, "%s", kernel_path);

				train(epoch_count, min_lr, max_lr, binpack_path, kernel_path);
				break;
			}
			#endif
			else if (strcmp(str, "quit") == 0) {
				quit = 1;
				if (is_searching) {
					is_ponder = 0;
					BROADCAST_COND_VAR(ponder_cond_var);
					abort_flag = 1;
					THREAD_WAIT(calculate_thread);
				}
				break;
			}

			str = _strtok(NULL, delim, &strtok_ptr);
		}
	}

	if (context->accumulator_stack)
		free(context->accumulator_stack);
	if (context->refresh_table)
		free(context->refresh_table);
	if (context)
		free(context);
	if(ponder_locks_initialized) {
		DESTROY_COND_VAR(ponder_cond_var);
		DESTROY_MUTEX(ponder_lock);
	}
	destroy_tt_table(transposition_table);
	tb_free();
	disable_debug_messages();  //closes file if open.
}

void ready_up(int *is_path_dirty, int *is_ready, char *SyzygyPath, thread_context *context)
{
	if (*is_ready)
		return;

	*is_ready = 1;
	if (zobrist_piece_keys[0][0] == 0)
		init_zobrist_piece_keys();

	init_search_tables();

	if (rook_table[0] == 0)
		initMagics();
	if (pawn_attacks[0][0] == 0)
		init_pawn_attacks();
	if (knight_attacks[0] == 0)
		init_knight_move_table();
	if (king_attacks[0] == 0)
		init_king_move_table();

	init_cuckoo_move_table();

	init_hce_tables();

	if (!transposition_table) {
		transposition_table = create_tt_table();
		context->tt = transposition_table;
	}

	init_nnue();
	if (use_nnue) {
		if (!context->accumulator_stack)
			context->accumulator_stack = calloc(MAX_PLY + 1, sizeof(accumulator));
		if (!context->refresh_table)
			context->refresh_table = calloc(1, sizeof(accumulator_refresh_table));
	} else {
		if (context->accumulator_stack) {
			free(context->accumulator_stack);
			context->accumulator_stack = NULL;
		}
		if (context->refresh_table) {
			free(context->refresh_table);
			context->refresh_table = NULL;
		}
	}

	if (*is_path_dirty) {
		tb_init(SyzygyPath);
		is_path_dirty = 0;
	}

	load_fen_string_to_board(&context->board_stack[0], STARTPOS_FEN, &context->repetitions);

	ponder_locks_initialized = 1;
	CREATE_COND_VAR(ponder_cond_var);
	CREATE_MUTEX(ponder_lock);
}
