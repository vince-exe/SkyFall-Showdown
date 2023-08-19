#pragma once

#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include "Entity.h"
#include "ui_utils.h"

class MenuConfirmationExit {
public:
	void init(std::shared_ptr<sf::RenderWindow> windowPtr, Entity& background, PopupReturnValues& checker, sf::Cursor& defCursor, sf::Cursor& pointCursor);
	
private:
	void handleMouseCursor(std::shared_ptr<sf::RenderWindow> windowPtr, sf::Cursor& pointCursor, sf::Cursor& defCursor);

	void handleButtonClicks(std::shared_ptr<sf::RenderWindow> windowPtr, PopupReturnValues& checker, bool& exitRequested);

	bool loadTextures();

	void renderWindow(std::shared_ptr<sf::RenderWindow> windowPtr, Entity& background);

private:
	Entity backBtn, exitBtn, text;
};


