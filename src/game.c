/*
 * game.c
 *
 *  Created on: 20 de nov de 2015
 *      Author: cassiano
 */

#include "game.h"

#define MOVEMENT_FACTOR 1.0
#define TIME_BETWEEN_SHOTS 50
#define MAX_ENEMIES_ON_SCREEN 20
#define MAX_DIVERS_ON_SCREEN 3

#define DEFAULT_VELOCITY_FACTOR 1.75
#define DEFAULT_DIVER_VELOCITY_FACTOR 0.75

#define DIVER_RESCUE_SCORE 60
#define ENEMY_DESTROY_SCORE 60

int zone_to_screen(Game * game, int zone);
int screen_to_zone(Game * game, int y);

void on_click_start(void * data) {
	if (data) {
		Game * game = (Game *) data;
		Game_start(game);
	}
}

void on_click_records(void * data) {
	if (data) {
		Game * game = (Game *) data;
		Game_display_records(game);
	}
}

void on_click_resume(void * data) {
	if (data) {
		Game * game = (Game *) data;
		game->is_paused = false;
	}
}

void on_click_back(void * data) {
	if (data) {
		Game * game = (Game *) data;
		game->is_displaying_records = false;
	}
}

void on_click_ok(void * data) {
	if (data) {
		Game * game = (Game *) data;

		Input * input = (Input *) game->new_record_menu->inputs->begin->value;

		if (strlen(input->text) > 0) {
			SDL_StopTextInput();

			const Score s = Score_create(input->text, game->player->score);
			Score_save(s);
			game->is_editing_new_record = false;
		}
	}
}

void on_click_quit(void * data) {
	if (data) {
		Game_destroy((Game *) data);
	}

	exit(0);
}

void on_click_exit(void * data) {
	if (data) {
		Game * game = (Game *) data;

		Game_stop(game);

		if (Score_is_new_record(game->player->score)) {
			SDL_StartTextInput();
			SDL_SetTextInputRect(
					((Input *) (game->new_record_menu->inputs->begin->value))->rect);

			game->is_editing_new_record = true;
		} else {
			Game_reset(game);
		}
	}
}

Game * Game_create(SDL_Window * window) {
	Game * game = (Game *) malloc(sizeof(Game));

	if (game != NULL) {

		game->player = Player_create(window, RES_PLAYER,
				(SCREEN_WIDTH - 15) / 2, (SCREEN_HEIGHT - 64) / 2,
				2 * MOVEMENT_FACTOR,
				TIME_BETWEEN_SHOTS);
		game->enemies = List_create();
		game->bullets = List_create();
		game->divers = List_create();
		game->window = window;
		game->enemies_on_screen = 0;
		game->divers_on_screen = 0;
		game->is_paused = false;
		game->surface = SDL_GetWindowSurface(window);

		game->score_color = (SDL_Color ) {0xFF, 0xFF, 00};

		SDL_Rect breathe_zone =
				{ 0, 0, game->surface->w, (game->surface->h / 6) };

		game->life_surface = LifeSurface_create(window, RES_PLAYER_ICON, 0, 0);

		game->enemy_on_surface = NULL;

		game->breathe_zone = breathe_zone;
		game->timer = Timer_create();

		game->ground_rect.x = 0;
		game->ground_rect.y = (5 * SCREEN_HEIGHT) / 6;
		game->ground_rect.w = SCREEN_WIDTH;
		game->ground_rect.h = SCREEN_HEIGHT / 6;

		game->spawn_zone_size = (SCREEN_HEIGHT
				- (game->breathe_zone.h + game->player->surface->h / 2)
				- game->ground_rect.h) / (game->player->sprite_rect->h + 20);

		game->zone_lock = (ZoneLock *) malloc(
				game->spawn_zone_size * sizeof(ZoneLock));

		int i;

		for (i = 0; i < game->spawn_zone_size; i++) {
			game->zone_lock[i] = (ZoneLock ) {false, 0, 0, 0};
		}

		game->explosion_sound = Mix_LoadWAV(RES_EXPLOSION_SOUND);
		game->rescue_sound = Mix_LoadWAV(RES_RESCUE_DIVER_SOUND);

		game->background_music = Mix_LoadMUS(RES_BACKGROUND_SOUND);

		Timer_start(game->timer);

		game->font = TTF_OpenFont(RES_DEFAULT_FONT, 28);

		game->score_rect = (SDL_Rect *) malloc(sizeof(SDL_Rect));

		game->is_started = false;
		game->is_editing_new_record = false;
		game->is_displaying_records = false;

		char temp[20];
		sprintf(temp, "%d", game->player->score);

		game->score_surface = TTF_RenderText_Solid(game->font, temp,
				game->score_color);

		if (game->score_surface) {
			game->score_rect->x = (SCREEN_WIDTH - game->score_surface->w) / 2;
			game->score_rect->y = 0;
			game->score_rect->w = game->score_surface->w;
			game->score_rect->h = game->score_surface->h;
		}

		int x, y;
		x = (SCREEN_WIDTH - 300) / 2;
		y = (5 * SCREEN_HEIGHT) / 6 + (game->ground_rect.h - 20) / 2;

		game->oxygen_bar = OxygenBar_create(window, x, y, 300, 20);

		SDL_Color color = { 0x33, 0x33, 0x33, 0xFF / 2 };

		game->pause_menu = Menu_create(window, NULL, color);
		game->main_menu = Menu_create(window, NULL, color);
		game->new_record_menu = Menu_create(window, NULL, color);
		game->records_menu = Menu_create(window, NULL, color);

		if (game->pause_menu) {
			Button * resume_button = Button_create(window, RES_RESUME,
					on_click_resume);
			Button * quit_button = Button_create(window, RES_EXIT,
					on_click_exit);

			Button_set_postition(resume_button,
					(SCREEN_WIDTH - resume_button->rect->w) / 2,
					(int) (SCREEN_HEIGHT / 2 - resume_button->rect->w * 1.2));
			Button_set_postition(quit_button,
					(SCREEN_WIDTH - quit_button->rect->w) / 2,
					(int) (SCREEN_HEIGHT / 2 + quit_button->rect->w * 1.2));

			Menu_add_button(game->pause_menu, resume_button);
			Menu_add_button(game->pause_menu, quit_button);
		}

		if (game->main_menu) {
			Button * start_button = Button_create(window, RES_START,
					on_click_start);
			Button * quit_button = Button_create(window, RES_QUIT,
					on_click_quit);
			Button * records_button = Button_create(window, RES_RECORDS,
					on_click_records);

			Button_set_postition(start_button,
					(SCREEN_WIDTH - start_button->rect->w) >> 1,
					(int) ((SCREEN_HEIGHT >> 1) - start_button->rect->w * 1.2));

			Button_set_postition(quit_button,
					(SCREEN_WIDTH - quit_button->rect->w) >> 1,
					(int) ((SCREEN_HEIGHT >> 1) + quit_button->rect->w * 1.2));

			Button_set_postition(records_button,
					(SCREEN_WIDTH - records_button->rect->w) >> 1,
					(SCREEN_HEIGHT - records_button->rect->h) >> 1);

			Menu_add_button(game->main_menu, start_button);
			Menu_add_button(game->main_menu, quit_button);
			Menu_add_button(game->main_menu, records_button);
		}

		if (game->records_menu) {
			Button * back_button = Button_create(window, RES_BACK,
					on_click_back);

			Button_set_postition(back_button, 50,
			SCREEN_HEIGHT - 50 - back_button->surface->h);

			Menu_add_button(game->records_menu, back_button);
		}

		color = (SDL_Color ) {0x33, 0x33, 0x33, 0xFF / 2};

		game->diver_icon = IMG_Load(RES_DIVER_ICON);

		int diver_w = game->diver_icon->w * MAX_DIVERS_FOR_RESCUE;
		int diver_y =
				game->oxygen_bar->rect->y + game->oxygen_bar->rect->h
						+ (SCREEN_HEIGHT
								- (game->oxygen_bar->rect->y
										+ game->oxygen_bar->rect->h)
								- game->diver_icon->h) / 2;

		int diver_x = (SCREEN_WIDTH - diver_w) / 2;

		game->diver_icon_rect = (SDL_Rect ) {diver_x, diver_y, diver_w, game->diver_icon->h};

		SDL_Rect rect = (SDL_Rect ) {(SCREEN_WIDTH - 400) / 2, 0, 400, 50};

		if (game->new_record_menu) {
			Input * input = Input_create(window,
					TTF_OpenFont(RES_DEFAULT_FONT, 28), &rect, color,
					game->score_color);

			Label * label = Label_create(window,
					TTF_OpenFont(RES_DEFAULT_FONT, 35), "YOUR NAME", 0, 0);

			Button * ok_button = Button_create(window, RES_OK, on_click_ok);

			input->rect->y = (SCREEN_HEIGHT >> 1) - 10 - input->rect->h;
			label->rect.y = input->rect->y - label->rect.h - 40;
			label->rect.x = (SCREEN_WIDTH - label->rect.w) >> 1;

			Button_set_postition(ok_button,
					(SCREEN_WIDTH - ok_button->rect->w) >> 1,
					(SCREEN_HEIGHT >> 1) + 10);

			Menu_add_input(game->new_record_menu, input);
			Menu_add_label(game->new_record_menu, label);
			Menu_add_button(game->new_record_menu, ok_button);
		}
	}

	return game;
}

void Game_display_records(Game * game) {
	if (game) {
		if (!game->is_displaying_records) {
			game->is_displaying_records = true;

			int i, size, y = 50;
			Score * scores = Score_load(&size);

			TTF_Font * font = TTF_OpenFont(RES_DEFAULT_FONT, 20);

			Menu_destroy_labels(game->records_menu);

			for (i = 0; i < size; ++i) {
				Label * name_label = Label_create(game->window, font,
						scores[i].name, 50, y);

				char temp[30];
				sprintf(temp, "%d", scores[i].value);

				Label * score_label = Label_create(game->window, font, temp, 0,
						y);

				score_label->rect.x = SCREEN_WIDTH - 50 - score_label->rect.w;

				y += 25;

				Menu_add_label(game->records_menu, name_label);
				Menu_add_label(game->records_menu, score_label);
			}
		}
		Menu_render(game->records_menu, game->surface);
	}
}

void Game_reset(Game * game) {
	if (game) {
		game->enemies_on_screen = 0;
		game->divers_on_screen = 0;
		game->player->score = 0;
		game->player->lifes = MAX_LIFES;
		game->player->is_dead = false;

		Player_set_position(game->player, (SCREEN_WIDTH - 15) / 2,
				(SCREEN_HEIGHT - 64) / 2);

		int i;

		for (i = 0; i < game->spawn_zone_size; i++) {
			game->zone_lock[i] = (ZoneLock ) {false, 0, 0, 0};
		}

		Game_destroy_bullets(game);
		Game_destroy_enemies(game);
		Game_destroy_divers(game);

		Game_destroy_enemy(game, game->enemy_on_surface);
	}
}

void Game_update_score_surface(Game * game) {
	char temp[20];
	sprintf(temp, "%d", game->player->score);

	SDL_FreeSurface(game->score_surface);

	game->score_surface = TTF_RenderText_Solid(game->font, temp,
			game->score_color);

	game->score_rect->x = (SCREEN_WIDTH - game->score_surface->w) / 2;
	game->score_rect->w = game->score_surface->w;
	game->score_rect->h = game->score_surface->h;
}

bool Game_is_player_breathing(Game * game) {

	SDL_Rect rect;
	SDL_bool b = SDL_IntersectRect(&game->breathe_zone, game->player->rect,
			&rect);

	return b ? true : false;
}

bool collision_check(SDL_Rect * element_1, CollisionMask mask_1,
		SDL_Rect * element_2, CollisionMask mask_2) {

	bool is_colliding = false;

	if (mask_1 ^ mask_2) {

		SDL_Rect rect;
		SDL_bool b = SDL_IntersectRect(element_1, element_2, &rect);

		is_colliding = b ? true : false;
	}

	return is_colliding;
}

void Game_spawn_enemy_on_surface(Game * game) {
	game->enemy_on_surface = Enemy_create(game->window, RES_ENEMY_SUBMARINE,
			SUBMARINE, LEFT, 0, DEFAULT_DIVER_VELOCITY_FACTOR, 0);

	game->enemy_on_surface->rect->y = game->breathe_zone.h
			- (game->enemy_on_surface->rect->h / 2);
}

Enemy * Game_spawn_enemy(Game * game) {

	Enemy * enemy = NULL;

	float probability = ((float) rand()) / INT32_MAX;

	if (probability < 0.025) {
		EnemyType enemy_type = (rand() > (INT32_MAX >> 1)) ? SUBMARINE : SHARK;
		Direction direction = (rand() > (INT32_MAX >> 1)) ? RIGHT : LEFT;

		Uint8 zone = rand() % game->spawn_zone_size;

		int y = zone_to_screen(game, zone);

		if (game->enemies_on_screen < MAX_ENEMIES_ON_SCREEN
				&& !game->zone_lock[zone].is_locked
				&& (game->zone_lock[zone].direction == 0
						|| game->zone_lock[zone].direction == direction)) {
			if (enemy_type == SHARK) {
				enemy = Enemy_create(game->window, RES_SHARK, enemy_type,
						direction, y, DEFAULT_VELOCITY_FACTOR, 0);
			} else if (enemy_type == SUBMARINE) {
				enemy = Enemy_create(game->window, RES_ENEMY_SUBMARINE,
						enemy_type, direction, y, DEFAULT_VELOCITY_FACTOR,
						TIME_BETWEEN_SHOTS);
			}

			if (enemy != NULL) {
				List_insert(game->enemies, (void *) enemy);
				game->enemies_on_screen++;

				game->zone_lock[zone].is_locked = true;

				if (game->zone_lock[zone].enemies_number == 0) {
					game->zone_lock[zone].direction = direction;
					game->zone_lock[zone].enemy_type = enemy_type;
				}

				game->zone_lock[zone].enemies_number++;
			}
		}
	}

	return enemy;
}

void Game_destroy_enemy(Game * game, Enemy * enemy) {
	if (enemy != game->enemy_on_surface) {
		int zone = screen_to_zone(game, enemy->rect->y);

		game->zone_lock[zone].enemies_number--;

		bool locked_zone[game->spawn_zone_size];

		int i;
		for (i = 0; i < game->spawn_zone_size; ++i) {
			locked_zone[i] = false;
		}

		List_remove(game->enemies, (void *) enemy);
		Enemy_destroy(enemy);

		Node * actual = game->enemies->begin;

		while (actual != NULL) {
			if (actual->value != NULL) {
				Node * prox = actual->next;
				Enemy * e = (Enemy *) actual->value;

				int zone = screen_to_zone(game, e->rect->y);

				if (e->rect->w < e->surface->w) {
					locked_zone[zone] = true;
				}

				actual = prox;
			}
		}

		for (i = 0; i < game->spawn_zone_size; ++i) {
			game->zone_lock[i].is_locked = locked_zone[i];
		}

		game->enemies_on_screen--;
	} else {
		Enemy_destroy(enemy);
		game->enemy_on_surface = NULL;
	}
}

void Game_destroy_bullet(Game * game, Bullet * bullet) {
	List_remove(game->bullets, (void *) bullet);
	Bullet_destroy(bullet);
}

Diver * Game_spawn_diver(Game * game) {

	Diver * diver = NULL;
	float probability = ((float) rand()) / INT32_MAX;

	if (probability < 0.005) {
		Direction direction = (rand() > (INT32_MAX >> 1)) ? RIGHT : LEFT;

		Uint8 zone = rand() % game->spawn_zone_size;

		int y = zone_to_screen(game, zone) + game->player->surface->h / 4;

		if (game->divers_on_screen < MAX_DIVERS_ON_SCREEN) {
			diver = Diver_create(game->window, DEFAULT_DIVER_VELOCITY_FACTOR,
					direction, y);
			List_insert(game->divers, diver);
			game->divers_on_screen++;
		}
	}

	return diver;
}

void Game_start(Game * game) {
	if (game) {
		game->is_started = true;
	}
}

void Game_stop(Game * game) {
	if (game) {
		game->is_started = false;
		game->is_paused = false;

		if (Mix_PlayingMusic()) {
			Mix_HaltMusic();
		}
	}
}

void Game_update(Game * game) {

	SDL_FillRect(game->surface, NULL,
			SDL_MapRGB(game->surface->format, 0x00, 0x66, 0xFF));

	if (game->is_started) {

		SDL_FillRect(game->surface, &game->breathe_zone,
				SDL_MapRGB(game->surface->format, 0x33, 0x33, 0xCC));

		if (!game->is_paused) {
			if (!Game_is_player_breathing(game)) {
				if (game->player->oxygen >= 0) {
					game->player->oxygen -= 0.04;

					if (game->player->oxygen <= 0) {
						game->player->oxygen = 0.0;

						Player_die(game->player);
						LifeSurface_set_lifes(game->life_surface,
								game->player->lifes);

						if (Player_is_dead(game->player)) {
							Game_stop(game);
							on_click_exit(game);
						}
					}
				}
			} else {
				if (game->player->oxygen < 100) {
					game->player->oxygen += 0.25;
					if (game->player->oxygen > 100)
						game->player->oxygen = 100;
				}

				if (game->player->divers_rescued == MAX_DIVERS_FOR_RESCUE) {
					game->player->divers_rescued = 0;
					game->player->score += DIVER_RESCUE_SCORE;

					Game_update_score_surface(game);
				}
			}
			OxygenBar_set_oxygen(game->oxygen_bar, game->player->oxygen);

			if (!Mix_PlayingMusic()) {
				Mix_PlayMusic(game->background_music, -1);
			} else if (Mix_PausedMusic()) {
				Mix_ResumeMusic();
			}
		} else {
			if (Mix_PlayingMusic()) {
				Mix_PauseMusic();
			}
		}

		Game_update_bullets(game);
		Game_update_divers(game);
		Game_update_enemies(game);

		if (!Player_is_dead(game->player)) {
			Player_render(game->player, game->surface);
		}

		SDL_FillRect(game->surface, &game->ground_rect,
				SDL_MapRGB(game->surface->format, 0x00, 0xCC, 0x66));

		OxygenBar_render(game->oxygen_bar, game->surface);

		Uint8 i;
		SDL_Rect diver_rect_temp = game->diver_icon_rect;
		diver_rect_temp.w /= MAX_DIVERS_FOR_RESCUE;

		for (i = 0; i < game->player->divers_rescued; ++i) {
			diver_rect_temp.x = game->diver_icon_rect.x + i * diver_rect_temp.w;

			SDL_BlitSurface(game->diver_icon, NULL, game->surface,
					&diver_rect_temp);
		}

		SDL_BlitSurface(game->score_surface, NULL, game->surface,
				game->score_rect);

		LifeSurface_render(game->life_surface, game->surface);

		if (game->is_paused) {
			Menu_render(game->pause_menu, game->surface);
		}

		Game_check_bullets_collision(game);
		Game_check_divers_collision(game);
		Game_check_enemies_collision(game);
	} else {
		if (game->is_editing_new_record) {
			Menu_render(game->new_record_menu, game->surface);

			SDL_Event e;
			Input * input =
					(Input *) game->new_record_menu->inputs->begin->value;

			if (SDL_PollEvent(&e)) {
				if (e.type == SDL_TEXTINPUT) {
					Input_insert_text(input, e.text.text);
				}
			}
			Menu_render(game->new_record_menu, game->surface);
		} else if (game->is_displaying_records) {
			Game_display_records(game);
		} else {
			Menu_render(game->main_menu, game->surface);
		}
	}

	SDL_UpdateWindowSurface(game->window);
}

void Game_check_divers_collision(Game * game) {
	if (!game->is_paused) {

		Node * node = game->divers->begin;
		Node * aux = NULL;

		if (game->player->divers_rescued < MAX_DIVERS_FOR_RESCUE) {
			while (node != NULL) {
				aux = node->next;

				Diver * diver = (Diver *) node->value;

				bool collision = collision_check(diver->rect,
						diver->collision_mask, game->player->rect,
						game->player->collision_mask);

				if (collision) {
					Game_destroy_diver(game, diver);
					Mix_PlayChannel(-1, game->rescue_sound, 0);
					game->player->divers_rescued++;
				}
				node = aux;
			}
		}
	}
}

void Game_check_enemies_collision(Game * game) {
	if (!game->is_paused) {

		bool collision = false;

		Node * node = game->enemies->begin;
		Node * aux = NULL;

		while (node != NULL) {
			aux = node->next;

			Enemy * enemy = (Enemy *) node->value;

			collision = collision_check(enemy->rect, enemy->collision_mask,
					game->player->rect, game->player->collision_mask);

			if (collision) {
				Game_destroy_enemy(game, enemy);
				Mix_PlayChannel(-1, game->explosion_sound, 0);

				Player_die(game->player);
				LifeSurface_set_lifes(game->life_surface, game->player->lifes);

				if (Player_is_dead(game->player)) {
					Game_stop(game);
					on_click_exit(game);
				}

				break;
			}
			node = aux;
		}

		if (!collision) {

			if (game->enemy_on_surface) {
				collision = collision_check(game->enemy_on_surface->rect,
						game->enemy_on_surface->collision_mask,
						game->player->rect, game->player->collision_mask);

				if (collision) {
					Game_destroy_enemy(game, game->enemy_on_surface);
					Mix_PlayChannel(-1, game->explosion_sound, 0);

					game->enemy_on_surface = NULL;
					// TODO Adicionar efeito da colisão aqui
					Player_die(game->player);
					LifeSurface_set_lifes(game->life_surface,
							game->player->lifes);

					if (Player_is_dead(game->player)) {
						Game_stop(game);
						on_click_exit(game);
					}
				}
			}
		}
	}
}

void Game_update_enemies(Game * game) {
	Node * actual = game->enemies->begin;

	bool locked_zone[game->spawn_zone_size];

	int i;
	for (i = 0; i < game->spawn_zone_size; ++i) {
		locked_zone[i] = false;
	}

	while (actual != NULL) {
		if (actual->value != NULL) {
			Node * prox = actual->next;
			Enemy * enemy = (Enemy *) actual->value;

			actual = prox;

			if (!game->is_paused) {
				Enemy_move(enemy);
			}

//			int zone = screen_to_zone(game, enemy->rect->y);
//
//			//if (enemy->rect->x + enemy->rect->w < SCREEN_WIDTH) {
//			if (enemy->real_x
//					> 0|| (enemy->real_x + enemy->surface->w) < SCREEN_WIDTH) {
//				locked_zone[zone] |= false;
//				/*
//				if (game->zone_lock[zone].enemies_number >= 1) {
//					game->zone_lock[zone].is_locked |= false;
//				} else {
//					game->zone_lock[zone].is_locked = false;
//				}
//				*/
//			} else {
//				locked_zone[zone] |= true;
//			}

			if (Enemy_is_entered_on_screen(enemy)) {
				if (Enemy_is_visible(enemy)) {
					Enemy_render(enemy, game->surface, game->bullets);

					int zone = screen_to_zone(game, enemy->rect->y);

					if (enemy->sprite_rect->w < enemy->surface->w) {
						locked_zone[zone] = true;
					}
				} else {
					Game_destroy_enemy(game, enemy);
				}
			}
		} else {
			actual = actual->next;
		}
	}

	for (i = 0; i < game->spawn_zone_size; ++i) {
		game->zone_lock[i].is_locked = locked_zone[i];
	}

	if (game->enemy_on_surface) {

		if (!game->is_paused) {
			Enemy_move(game->enemy_on_surface);
		}

		if (Enemy_is_visible(game->enemy_on_surface)) {
			Enemy_render(game->enemy_on_surface, game->surface, NULL);
		} else {
			Game_destroy_enemy(game, game->enemy_on_surface);
			game->enemy_on_surface = NULL;
		}
	}
}

void Game_update_bullets(Game * game) {
	Node * node = game->bullets->begin;

	while (node != NULL) {

		Bullet * bullet = (Bullet *) node->value;

		node = node->next;

		if (!game->is_paused) {
			Bullet_move(bullet);
		}

		if (Bullet_is_visible(bullet)) {
			Bullet_render(bullet, game->surface);
		} else {
			List_remove(game->bullets, bullet);
			Bullet_destroy(bullet);
		}
	}
}

void Game_check_bullets_collision(Game * game) {

	if (!game->is_paused) {

		Node * node = game->bullets->begin;
		Node * aux = NULL;

		Player * player = game->player;

		while (node != NULL) {
			aux = node->next;

			Bullet * bullet = (Bullet *) node->value;

			Node * node_enemy = game->enemies->begin;
			Node * aux_enemy = NULL;

			while (node_enemy != NULL) {

				aux_enemy = node_enemy->next;

				Enemy * enemy = (Enemy *) node_enemy->value;

				bool collision = collision_check(bullet->rect,
						bullet->collision_mask, enemy->rect,
						enemy->collision_mask);

				if (collision) {
					if (enemy->type == SUBMARINE) {
						Mix_PlayChannel(-1, game->explosion_sound, 0);
					}

					Game_destroy_enemy(game, enemy);
					Game_destroy_bullet(game, bullet);

					bullet = NULL;
					enemy = NULL;

					player->score += ENEMY_DESTROY_SCORE;

					Game_update_score_surface(game);

					break;
				}
				node_enemy = aux_enemy;
			}

			if (bullet
					&& collision_check(bullet->rect, bullet->collision_mask,
							player->rect, player->collision_mask)) {
				Game_destroy_bullet(game, bullet);
				Mix_PlayChannel(-1, game->explosion_sound, 0);
				Player_die(player);
				LifeSurface_set_lifes(game->life_surface, game->player->lifes);

				if (Player_is_dead(player)) {
					Game_stop(game);
					on_click_exit(game);
				}
			}

			node = aux;
		}
	}
}

void Game_update_divers(Game * game) {
	Node * actual = game->divers->begin;

	while (actual != NULL) {

		if (actual->value != NULL) {
			Node * prox = actual->next;
			Diver * diver = (Diver *) actual->value;

			actual = prox;

			if (!game->is_paused) {
				Diver_move(diver);

				Uint8 zone_number = screen_to_zone(game, diver->rect->y);
				ZoneLock zone = game->zone_lock[zone_number];

				if (zone.enemies_number != 0) { //&& zone.direction != diver->direction) {
//					float r = ((float) rand()) / RAND_MAX;
//
//					if (r < 0.005) {
//						Diver_change_direction(diver);
//					}

					bool is_colliding = false;

					Node * enemy_actual = game->enemies->begin;

					while (enemy_actual) {
						Node * enemy_prox = enemy_actual->next;
						Enemy * enemy = (Enemy *) enemy_actual->value;

						if (screen_to_zone(game, enemy->rect->y)
								== zone_number) {

							is_colliding = collision_check(diver->rect,
									diver->collision_mask, enemy->rect,
									enemy->collision_mask);

							if (is_colliding) {
								if (diver->direction != enemy->direction) {
									Diver_change_direction(diver);
								}

								diver->movement_factor = enemy->movement_factor;

								break;
							}
						}

						enemy_actual = enemy_prox;
					}

					if (!is_colliding) {
						diver->movement_factor = DEFAULT_DIVER_VELOCITY_FACTOR;
					}
				}
			}

			if (Diver_is_visible(diver)) {
				Diver_render(diver, game->surface);
			} else {
				Game_destroy_diver(game, diver);
			}

		} else {
			actual = actual->next;
		}
	}
}

void Game_destroy_diver(Game * game, Diver * diver) {
	List_remove(game->divers, (void *) diver);
	Diver_destroy(diver);

	game->divers_on_screen--;
}

void Game_destroy_divers(Game * game) {
	Node * node = game->divers->begin;
	Node * aux = NULL;

	while (node != NULL) {
		aux = node->next;

		if (node->value != NULL) {
			Diver_destroy((Diver *) node->value);
		}

		node = aux;
	}

	game->divers->begin = NULL;

	game->divers_on_screen = 0;
}

void Game_destroy_enemies(Game* game) {
	Node* actual = game->enemies->begin;
	Node * aux = NULL;
	while (actual != NULL) {

		aux = actual->next;

		if (actual->value != NULL) {
			Enemy_destroy((Enemy *) actual->value);
		}
		actual = aux;
	}

	game->enemies->begin = NULL;

	game->enemies_on_screen = 0;
}

void Game_destroy_bullets(Game* game) {
	Node* actual = game->bullets->begin;
	Node * aux = NULL;
	while (actual != NULL) {

		aux = actual->next;

		if (actual->value != NULL) {
			Bullet_destroy((Bullet *) actual->value);
		}
		actual = aux;
	}

	game->bullets->begin = NULL;
}

void Game_destroy(Game * game) {

	if (game != NULL) {
		Player_destroy(game->player);

		Game_destroy_enemies(game);
		Enemy_destroy(game->enemy_on_surface);
		Game_destroy_bullets(game);
		Game_destroy_divers(game);
		Node * actual = game->bullets->begin;
		Node * aux = NULL;

		while (actual != NULL) {

			aux = actual->next;

			if (actual->value != NULL) {
				Bullet_destroy((Bullet *) actual->value);
			}
			actual = aux;
		}
		List_destroy(game->enemies);
		List_destroy(game->bullets);
		List_destroy(game->divers);

		SDL_FreeSurface(game->score_surface);
		free(game->score_rect);
		free(game->zone_lock);

		SDL_FreeSurface(game->diver_icon);

		TTF_CloseFont(game->font);
		Mix_FreeChunk(game->explosion_sound);
		Mix_FreeChunk(game->rescue_sound);
		Mix_FreeMusic(game->background_music);
		Timer_destroy(game->timer);

		Menu_destroy(game->pause_menu);
		Menu_destroy(game->main_menu);
		Menu_destroy(game->new_record_menu);

		LifeSurface_destroy(game->life_surface);

		free(game);
	}
}

int zone_to_screen(Game * game, int zone) {
	return ((game->player->surface->h + 20) * zone + game->breathe_zone.h)
			+ (game->player->surface->h / 2);
}

int screen_to_zone(Game * game, int y) {
	return (y - game->breathe_zone.h + (game->player->surface->h / 2))
			/ (game->player->surface->h + 20);
}
