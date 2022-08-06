/*
Copyright © 2011-2012 Thane Brimhall
Copyright © 2013 Henrik Andersson
Copyright © 2013-2016 Justin Jacobs

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
 * class MessageEngine
 *
 * The MessageEngine class loads all of FLARE's internal messages from a configuration file
 * and returns them as human-readable strings.
 *
 * This class is primarily used for making sure FLARE is flexible and translatable.
 */

#include "CommonIncludes.h"
#include "GetText.h"
#include "MessageEngine.h"
#include "ModManager.h"
#include "RenderDevice.h"
#include "SharedResources.h"
#include "Settings.h"

#include <stdarg.h>

MessageEngine::MessageEngine() {
	Utils::logInfo("MessageEngine: Using language '%s'", settings->language.c_str());

	GetText infile;

	std::vector<std::string> engineFiles = mods->list("languages/engine." + settings->language + ".po", ModManager::LIST_FULL_PATHS);
	if (engineFiles.empty() && settings->language != "en")
		Utils::logError("MessageEngine: Unable to open basic translation files located in languages/engine.%s.po", settings->language.c_str());

	for (unsigned i = 0; i < engineFiles.size(); ++i) {
		if (infile.open(engineFiles[i])) {
			while (infile.next()) {
				if (!infile.fuzzy)
					messages.insert(std::pair<std::string, std::string>(infile.key, infile.val));
			}
			infile.close();
		}
	}

	std::vector<std::string> dataFiles = mods->list("languages/data." + settings->language + ".po", ModManager::LIST_FULL_PATHS);
	if (dataFiles.empty() && settings->language != "en")
		Utils::logError("MessageEngine: Unable to open basic translation files located in languages/data.%s.po", settings->language.c_str());

	for (unsigned i = 0; i < dataFiles.size(); ++i) {
		if (infile.open(dataFiles[i])) {
			while (infile.next()) {
				if (!infile.fuzzy)
					messages.insert(std::pair<std::string, std::string>(infile.key, infile.val));
			}
			infile.close();
		}
	}
}

/**
 * This get() function is maintained for the purpose of strings that don't expect C/printf-style formatting.
 * We have allowed strings in mod data to not require the escaping of '%', so we can't pass such strings to getv() without issues.
 * We also use this where possible for engine strings, since it should be more efficient than rebuilding the string as getv() does.
 */
std::string MessageEngine::get(const std::string& key) {
	std::string message = messages[key];
	if (message == "") message = key;
	return unescape(message);
}

// NOTE: key is not passed by reference because doing so would result in undefined behavior when using va_start()
std::string MessageEngine::getv(const std::string key, ...) {
	std::string message = messages[key];
	if (message == "") message = key;

	va_list args;
	const char* format = message.c_str();
	const size_t buffer_size = 8192;
	char buffer[buffer_size];

	va_start(args, key);
	vsnprintf(buffer, buffer_size, format, args);
	va_end(args);

	return std::string(buffer);
}

// unescape c formatted string
std::string MessageEngine::unescape(const std::string& _val) {
	std::string val = _val;

	// unescape percentage %% to %
	size_t pos;
	while ((pos = val.find("%%")) != std::string::npos)
		val = val.replace(pos, 2, "%");

	return val;
}
