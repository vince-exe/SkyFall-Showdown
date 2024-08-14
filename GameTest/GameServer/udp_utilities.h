#pragma once

#include <iostream>
#include <string>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace UdpUtils {
	namespace errors {
		class InvalidBytesLength : public std::runtime_error {
		public:
			InvalidBytesLength(const std::string& message) : std::runtime_error(message) {} ;
		};
	}

	enum class GameMessageType {
		PLAYER_POSITION
	};
	
	typedef struct GameMessage {
		boost::uuids::uuid m_gameSessionID;
		std::string m_playerUsername;
		std::vector<uint8_t> data;
	}GameMessage;

	void m_serializeString(const std::string& str, std::vector<uint8_t>& buffer);

	std::string m_deserializeString(const std::vector<uint8_t>& buffer, size_t& offset);

	void floatArrToUint8tVec(std::vector<uint8_t>& vec, float* floatArray);
	
	bool uint8tVecToFloatArr(const std::vector<uint8_t>& vec, float* floatArray);
	
	std::vector<uint8_t> serializeUDPMessage(const GameMessage& message);

	GameMessage deserializeUDPMessage(const std::vector<uint8_t>& buffer);
}