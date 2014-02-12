/*
Copyright © 2011-2012 Clint Bellanger

This file is part of FLARE.

FLARE is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

FLARE is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
FLARE.  If not, see http://www.gnu.org/licenses/
*/

/**
 * GameStateLoad
 */

#include "Avatar.h"
#include "FileParser.h"
#include "GameStateLoad.h"
#include "GameStateTitle.h"
#include "GameStatePlay.h"
#include "GameStateNew.h"
#include "ItemManager.h"
#include "MenuConfirm.h"
#include "SharedResources.h"
#include "Settings.h"
#include "UtilsFileSystem.h"
#include "UtilsParsing.h"

using namespace std;


GameStateLoad::GameStateLoad() : GameState() {
	items = new ItemManager();
	loading_requested = false;
	loading = false;
	loaded = false;

	label_loading = new WidgetLabel();

	for (int i = 0; i < GAME_SLOT_MAX; i++) {
		label_name[i] = new WidgetLabel();
		label_level[i] = new WidgetLabel();
		label_map[i] = new WidgetLabel();
	}

	// Confirmation box to confirm deleting
	confirm = new MenuConfirm(msg->get("Delete Save"), msg->get("Delete this save?"));
	button_exit = new WidgetButton("images/menus/buttons/button_default.png");
	button_exit->label = msg->get("Exit to Title");
	button_exit->pos.x = VIEW_W_HALF - button_exit->pos.w/2;
	button_exit->pos.y = VIEW_H - button_exit->pos.h;
	button_exit->refresh();

	button_action = new WidgetButton("images/menus/buttons/button_default.png");
	button_action->label = msg->get("Choose a Slot");
	button_action->enabled = false;

	button_alternate = new WidgetButton("images/menus/buttons/button_default.png");
	button_alternate->label = msg->get("Delete Save");
	button_alternate->enabled = false;

	// Set up tab list
	tablist = TabList(HORIZONTAL);
	tablist.add(button_exit);

	// Read positions from config file
	FileParser infile;

	if (infile.open("menus/gameload.txt")) {
		while (infile.next()) {
			if (infile.key == "action_button") {
				button_action->pos.x = eatFirstInt(infile.val);
				button_action->pos.y = eatFirstInt(infile.val);
			}
			else if (infile.key == "alternate_button") {
				button_alternate->pos.x = eatFirstInt(infile.val);
				button_alternate->pos.y = eatFirstInt(infile.val);
			}
			else if (infile.key == "portrait") {
				Rect p_rect = toRect(infile.val);
				portrait.setDestX(p_rect.x + (VIEW_W - FRAME_W)/2);
				portrait.setDestY(p_rect.y + (VIEW_H - FRAME_H)/2);
				portrait.setClipW(p_rect.w);
				portrait.setClipH(p_rect.h);
			}
			else if (infile.key == "gameslot") {
				gameslot_pos = toRect(infile.val);
			}
			else if (infile.key == "preview") {
				preview_pos = toRect(infile.val);
				// label positions within each slot
			}
			else if (infile.key == "name") {
				name_pos = eatLabelInfo(infile.val);
			}
			else if (infile.key == "level") {
				level_pos = eatLabelInfo(infile.val);
			}
			else if (infile.key == "map") {
				map_pos = eatLabelInfo(infile.val);
			}
			else if (infile.key == "loading_label") {
				loading_pos = eatLabelInfo(infile.val);
				// Position for the avatar preview image in each slot
			}
			else if (infile.key == "sprite") {
				sprites_pos = toPoint(infile.val);
			}
		}
		infile.close();
	}

	// get displayable types list
	bool found_layer = false;
	if (infile.open("engine/hero_layers.txt")) {
		while(infile.next()) {
			if (infile.key == "layer") {
				unsigned dir = eatFirstInt(infile.val);
				if (dir != 6) continue;
				else found_layer = true;

				string layer = eatFirstString(infile.val);
				while (layer != "") {
					preview_layer.push_back(layer);
					layer = eatFirstString(infile.val);
				}
			}
		}
		infile.close();
	}
	if (!found_layer) fprintf(stderr, "Warning: Could not find layers for direction 6\n");

	button_action->pos.x += (VIEW_W - FRAME_W)/2;
	button_action->pos.y += (VIEW_H - FRAME_H)/2;
	button_action->refresh();

	button_alternate->pos.x += (VIEW_W - FRAME_W)/2;
	button_alternate->pos.y += (VIEW_H - FRAME_H)/2;
	button_alternate->refresh();

	load_game = false;

	for (int i=0; i<GAME_SLOT_MAX; i++) {
		current_map[i] = "";
	}

	loadGraphics();
	readGameSlots();

	for (int i=0; i<GAME_SLOT_MAX; i++) {
		slot_pos[i].x = gameslot_pos.x + (VIEW_W - FRAME_W)/2;
		slot_pos[i].h = gameslot_pos.h;
		slot_pos[i].y = gameslot_pos.y + (VIEW_H - FRAME_H)/2 + (i * gameslot_pos.h);
		slot_pos[i].w = gameslot_pos.w;
	}

	selected_slot = -1;

	// temp
	current_frame = 0;
	frame_ticker = 0;

	color_normal = font->getColor("menu_normal");
}

void GameStateLoad::loadGraphics() {
	background.setGraphics(render_device->loadGraphicSurface("images/menus/game_slots.png"));
	selection.setGraphics(render_device->loadGraphicSurface("images/menus/game_slot_select.png", "Couldn't load image", false, true));
	portrait_border.setGraphics(render_device->loadGraphicSurface("images/menus/portrait_border.png", "Couldn't load image", false, true));
}

void GameStateLoad::loadPortrait(int slot) {
	portrait.clearGraphics();

	if (slot < 0) return;

	if (stats[slot].name == "") return;

	portrait.setGraphics(
		render_device->loadGraphicSurface("images/portraits/" + stats[slot].gfx_portrait + ".png")
	);
}

void GameStateLoad::readGameSlots() {
	for (int i=0; i<GAME_SLOT_MAX; i++) {
		readGameSlot(i);
	}
}

string GameStateLoad::getMapName(const string& map_filename) {
	FileParser infile;
	if (!infile.open(map_filename, true, true, "")) return "";
	string map_name = "";

	while (map_name == "" && infile.next()) {
		if (infile.key == "title")
			map_name = msg->get(infile.val);
	}

	infile.close();
	return map_name;
}

void GameStateLoad::readGameSlot(int slot) {

	stringstream filename;
	FileParser infile;

	// abort if not a valid slot number
	if (slot < 0 || slot >= GAME_SLOT_MAX) return;

	// save slots are named save#.txt
	filename << PATH_USER;
	if (GAME_PREFIX.length() > 0)
		filename << GAME_PREFIX << "_";
	filename << "save" << (slot+1) << ".txt";

	if (!infile.open(filename.str(),false, true, "")) return;

	while (infile.next()) {

		// load (key=value) pairs
		if (infile.key == "name")
			stats[slot].name = infile.val;
		else if (infile.key == "class")
			stats[slot].character_class = infile.val;
		else if (infile.key == "xp")
			stats[slot].xp = toInt(infile.val);
		else if (infile.key == "build") {
			stats[slot].physical_character = toInt(infile.nextValue());
			stats[slot].mental_character = toInt(infile.nextValue());
			stats[slot].offense_character = toInt(infile.nextValue());
			stats[slot].defense_character = toInt(infile.nextValue());
		}
		else if (infile.key == "equipped") {
			string repeat_val = infile.nextValue();
			while (repeat_val != "") {
				equipped[slot].push_back(toInt(repeat_val));
				repeat_val = infile.nextValue();
			}
		}
		else if (infile.key == "option") {
			stats[slot].gfx_base = infile.nextValue();
			stats[slot].gfx_head = infile.nextValue();
			stats[slot].gfx_portrait = infile.nextValue();
		}
		else if (infile.key == "spawn") {
			current_map[slot] = getMapName(infile.nextValue());
		}
		else if (infile.key == "permadeath") {
			stats[slot].permadeath = toBool(infile.val);
		}
	}
	infile.close();

	stats[slot].recalc();
	loadPreview(slot);

}

void GameStateLoad::loadPreview(int slot) {

	vector<string> img_gfx;

	for (unsigned int i=0; i<sprites[slot].size(); i++) {
		sprites[slot][i].clearGraphics();
	}
	sprites[slot].clear();

	// fall back to default if it exists
	for (unsigned int i=0; i<preview_layer.size(); i++) {
		bool exists = fileExists(mods->locate("animations/avatar/" + stats[slot].gfx_base + "/default_" + preview_layer[i] + ".txt"));
		if (exists) {
			img_gfx.push_back("default_" + preview_layer[i]);
		}
		else if (preview_layer[i] == "head") {
			img_gfx.push_back(stats[slot].gfx_head);
		}
		else {
			img_gfx.push_back("");
		}
	}

	for (unsigned int i=0; i<equipped[slot].size(); i++) {
		if ((unsigned)equipped[slot][i] > items->items.size()-1) {
			fprintf(stderr, "Item in save slot %d with id=%d is out of bounds 1-%d. Your savegame is broken or you might be using an incompatible savegame/mod\n", slot+1, equipped[slot][i], (int)items->items.size()-1);
			continue;
		}
		vector<string>::iterator found = find(preview_layer.begin(), preview_layer.end(), items->items[equipped[slot][i]].type);
		if (equipped[slot][i] > 0 && found != preview_layer.end()) {
			img_gfx[distance(preview_layer.begin(), found)] = items->items[equipped[slot][i]].gfx;
		}
	}

	// composite the hero graphic
	sprites[slot].resize(img_gfx.size());
	for (unsigned int i=0; i<img_gfx.size(); i++) {
		if (img_gfx[i] == "")
			continue;

		if (!TEXTURE_QUALITY) {
			string fname = "images/avatar/" + stats[slot].gfx_base + "/preview/noalpha/" + img_gfx[i] + ".png";
			sprites[slot][i].setGraphics(
				render_device->loadGraphicSurface(fname, "Falling back to alpha version", false, true)
			);
		}
		if (sprites[slot][i].graphicsIsNull()) {
			sprites[slot][i].setGraphics(
				render_device->loadGraphicSurface("images/avatar/" + stats[slot].gfx_base + "/preview/" + img_gfx[i] + ".png")
			);
		}
		sprites[slot][i].setClip(0,0,sprites[slot][i].getGraphicsWidth(),sprites[slot][i].getGraphicsHeight());
	}

}


void GameStateLoad::logic() {

	frame_ticker++;
	if (frame_ticker == 64) frame_ticker = 0;
	if (frame_ticker < 32)
		current_frame = frame_ticker / 8;
	else
		current_frame = (63 - frame_ticker) / 8;

	if (!confirm->visible) {
		tablist.logic();
		if (button_exit->checkClick() || (inpt->pressing[CANCEL] && !inpt->lock[CANCEL])) {
			inpt->lock[CANCEL] = true;
			delete requestedGameState;
			requestedGameState = new GameStateTitle();
		}

		if (loading_requested) {
			loading = true;
			loading_requested = false;
			logicLoading();
		}

		if (button_action->checkClick()) {
			if (stats[selected_slot].name == "") {
				// create a new game
				GameStateNew* newgame = new GameStateNew();
				newgame->game_slot = selected_slot + 1;
				requestedGameState = newgame;
			}
			else {
				loading_requested = true;
			}
		}
		if (button_alternate->checkClick()) {
			// Display pop-up to make sure save should be deleted
			confirm->visible = true;
			confirm->render();
		}
		// check clicking game slot
		if (inpt->pressing[MAIN1] && !inpt->lock[MAIN1]) {
			for (int i=0; i<GAME_SLOT_MAX; i++) {
				if (isWithin(slot_pos[i], inpt->mouse)) {
					inpt->lock[MAIN1] = true;
					selected_slot = i;
					updateButtons();
				}
			}
		}

		// Allow characters to be navigateable via up/down keys
		if (inpt->pressing[UP] && !inpt->lock[UP]) {
			inpt->lock[UP] = true;
			selected_slot = (--selected_slot < 0) ? GAME_SLOT_MAX - 1 : selected_slot;
			updateButtons();
		}

		if (inpt->pressing[DOWN] && !inpt->lock[DOWN]) {
			inpt->lock[DOWN] = true;
			selected_slot = (++selected_slot == GAME_SLOT_MAX) ? 0 : selected_slot;
			updateButtons();
		}

	}
	else if (confirm->visible) {
		confirm->logic();
		if (confirm->confirmClicked) {
			stringstream filename;
			filename.str("");
			filename << PATH_USER;
			if (GAME_PREFIX.length() > 0)
				filename << GAME_PREFIX << "_";
			filename << "save" << (selected_slot+1) << ".txt";

			if (remove(filename.str().c_str()) != 0)
				perror("Error deleting save from path");

			if (stats[selected_slot].permadeath) {
				// Remove stash
				stringstream ss;
				ss.str("");
				ss << PATH_USER;
				if (GAME_PREFIX.length() > 0)
					ss << GAME_PREFIX << "_";
				ss << "stash_HC" << (selected_slot+1) << ".txt";
				if (remove(ss.str().c_str()) != 0)
					fprintf(stderr, "Error deleting hardcore stash in slot %d\n", selected_slot+1);
			}

			stats[selected_slot] = StatBlock();
			readGameSlot(selected_slot);

			updateButtons();

			confirm->visible = false;
			confirm->confirmClicked = false;
		}
	}
}

void GameStateLoad::logicLoading() {
	// load an existing game
	GameStatePlay* play = new GameStatePlay();
	play->resetGame();
	play->game_slot = selected_slot + 1;
	play->loadGame();
	requestedGameState = play;
	loaded = true;
	loading = false;
}

void GameStateLoad::updateButtons() {
	loadPortrait(selected_slot);

	if (button_action->enabled == false) {
		button_action->enabled = true;
		tablist.add(button_action);
	}
	button_action->tooltip = "";
	if (stats[selected_slot].name == "") {
		button_action->label = msg->get("New Game");
		if (!fileExists(mods->locate("maps/spawn.txt"))) {
			button_action->enabled = false;
			tablist.remove(button_action);
			button_action->tooltip = msg->get("Enable a story mod to continue");
		}
		button_alternate->enabled = false;
		tablist.remove(button_alternate);
	}
	else {
		if (button_alternate->enabled == false) {
			button_alternate->enabled = true;
			tablist.add(button_alternate);
		}
		button_action->label = msg->get("Load Game");
		if (current_map[selected_slot] == "") {
			if (!fileExists(mods->locate("maps/spawn.txt"))) {
				button_action->enabled = false;
				tablist.remove(button_action);
				button_action->tooltip = msg->get("Enable a story mod to continue");
			}
		}
	}
	button_action->refresh();
	button_alternate->refresh();
}

void GameStateLoad::render() {

	Rect src;
	Rect dest;

	// display background
	src.w = gameslot_pos.w;
	src.h = gameslot_pos.h * GAME_SLOT_MAX;
	src.x = src.y = 0;
	dest.x = slot_pos[0].x;
	dest.y = slot_pos[0].y;
	background.setClip(src);
	background.setDest(dest);
	render_device->render(background);

	// display selection
	if (selected_slot >= 0) {
		selection.setDest(slot_pos[selected_slot]);
		render_device->render(selection);
	}


	// portrait
	if (selected_slot >= 0 && !portrait.graphicsIsNull()) {
		render_device->render(portrait);
		dest.x = portrait.getDest().x;
		dest.y = portrait.getDest().y;
		portrait_border.setDest(dest);
		render_device->render(portrait_border);
	}

	Point label;
	stringstream ss;

	if (loading_requested || loading || loaded) {
		label.x = loading_pos.x + (VIEW_W - FRAME_W)/2;
		label.y = loading_pos.y + (VIEW_H - FRAME_H)/2;

		if ( loaded) {
			label_loading->set(msg->get("Entering game world..."));
		}
		else {
			label_loading->set(msg->get("Loading saved game..."));
		}

		label_loading->set(label.x, label.y, loading_pos.justify, loading_pos.valign, label_loading->get(), color_normal, loading_pos.font_style);
		label_loading->render();
	}

	Color color_permadeath_enabled = font->getColor("hardcore_color_name");
	// display text
	for (int slot=0; slot<GAME_SLOT_MAX; slot++) {
		if (stats[slot].name != "") {
			Color color_used = stats[slot].permadeath ? color_permadeath_enabled : color_normal;

			// name
			label.x = slot_pos[slot].x + name_pos.x;
			label.y = slot_pos[slot].y + name_pos.y;
			label_name[slot]->set(label.x, label.y, name_pos.justify, name_pos.valign, stats[slot].name, color_used, name_pos.font_style);
			label_name[slot]->render();

			// level
			ss.str("");
			label.x = slot_pos[slot].x + level_pos.x;
			label.y = slot_pos[slot].y + level_pos.y;
			ss << msg->get("Level %d %s", stats[slot].level, msg->get(stats[slot].character_class));
			if (stats[slot].permadeath)
				ss << ", " + msg->get("Permadeath");
			label_level[slot]->set(label.x, label.y, level_pos.justify, level_pos.valign, ss.str(), color_normal, level_pos.font_style);
			label_level[slot]->render();

			// map
			label.x = slot_pos[slot].x + map_pos.x;
			label.y = slot_pos[slot].y + map_pos.y;
			label_map[slot]->set(label.x, label.y, map_pos.justify, map_pos.valign, current_map[slot], color_normal, map_pos.font_style);
			label_map[slot]->render();

			// render character preview
			dest.x = slot_pos[slot].x + sprites_pos.x;
			dest.y = slot_pos[slot].y + sprites_pos.y;
			src.x = current_frame * preview_pos.h;
			src.y = 0;
			src.w = src.h = preview_pos.h;

			for (unsigned int i=0; i<sprites[slot].size(); i++) {
				sprites[slot][i].setClip(src);
				sprites[slot][i].setDest(dest);
				render_device->render(sprites[slot][i]);
			}
		}
		else {
			label.x = slot_pos[slot].x + name_pos.x;
			label.y = slot_pos[slot].y + name_pos.y;
			label_name[slot]->set(label.x, label.y, name_pos.justify, name_pos.valign, msg->get("Empty Slot"), color_normal, name_pos.font_style);
			label_name[slot]->render();
		}
	}
	// display warnings
	if (confirm->visible) confirm->render();

	// display buttons
	button_exit->render();
	button_action->render();
	button_alternate->render();
}

GameStateLoad::~GameStateLoad() {
	background.clearGraphics();
	selection.clearGraphics();
	portrait_border.clearGraphics();
	portrait.clearGraphics();

	delete button_exit;
	delete button_action;
	delete button_alternate;
	delete items;
	for (int slot=0; slot<GAME_SLOT_MAX; slot++) {
		for (unsigned int i=0; i<sprites[slot].size(); i++) {
			sprites[slot][i].clearGraphics();
		}
		sprites[slot].clear();
	}
	for (int i=0; i<GAME_SLOT_MAX; i++) {
		delete label_name[i];
		delete label_level[i];
		delete label_map[i];
	}
	delete label_loading;
	delete confirm;
}
