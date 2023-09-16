#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <utility>
#include <iostream>
#include <thread>

#include "Entity.h"
#include "ui_utils.h"
#include "MainMenuTextureManager.h"
#include "FontManager.h"
#include "Sound.h"

class IpPortMenu {
public:
	std::pair<std::string, int> init(std::shared_ptr<sf::RenderWindow> windowPtr, std::shared_ptr<Sound> notificationSound, Entity& background, PopupReturnValues& checker, sf::Cursor& defCursor, sf::Cursor& pointCursor);

private:
	void setTextures();

	void initSprites();

	void draw(Entity& background);

	void handleTextEntered(sf::Event& event);

	void handleMouseCursor(sf::Cursor& pointCursor, sf::Cursor& defCursor);

	void handleMouseButtons(sf::Event& event, PopupReturnValues& checker, bool& exitRequested, std::shared_ptr<Sound> notificationSound);

	bool setIpPort(std::string ipPort);

	void displayTextFuncTime(Entity& entity, int seconds);

private:
	std::shared_ptr<sf::RenderWindow> windowPtr;
	std::pair<std::string, int> pair;

	sf::Text inputDisplay;
	sf::RectangleShape line;

	Entity text;
	Entity connectBtn, cancelBtn;

	Entity entityToDisplay;
	bool displayText;
	Entity* msgToDisplay;
};
