/*
 * player.h
 *
 *  Created on: 30 de out de 2015
 *      Author: cassiano
 */

#include "types.h"
#include "bullet.h"
#include "linked_list.h"

#define MAX_DIVERS_FOR_RESCUE 5
#define MAX_LIFES 3
#define MAX_BLINK 10

#ifndef SRC_PLAYER_H_
#define SRC_PLAYER_H_

typedef struct _Player {
	SDL_Window * window;
	SDL_Rect * rect;
	float oxygen;
	int score;

	bool is_dead;
	Uint8 lifes;
	bool is_blinking;
	Uint8 blink_count;

	Uint8 divers_rescued;
	SDL_Rect * sprite_rect;
	SDL_Surface * surface;
	Uint32 time_between_shots;
	Uint32 time_shot_counter;
	Direction look_dir;

	CollisionMask collision_mask;
	float movement_factor;
} Player;

/**
 * Cria um jogador carregando uma imagem na posição (0, 0)
 */
Player * Player_create(SDL_Window * window, const char * filename, int x, int y,
		float movement_factor, Uint32 time_between_shots);

/**
 * Renderiza o player na SDL_Surface pai
 */
void Player_render(Player* player, SDL_Surface* parent);

void Player_shot(Player* player, List * bullets);

/**
 * Move o player na horizontal e na vertical
 */
void Player_move(Player * player, int h, int v, int x_max, int y_min, int y_max);

void Player_set_position(Player * player, int x, int y);

bool Player_is_dead(Player * player);

void Player_die(Player * player);

void Player_destroy(Player * player);

#endif /* SRC_PLAYER_H_ */
