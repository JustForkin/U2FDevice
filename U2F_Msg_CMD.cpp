#include "U2F_Msg_CMD.hpp"
#include "APDU.hpp"
#include "U2F_Register_APDU.hpp"
#include "U2FMessage.hpp"
#include "u2f.hpp"
#include "APDU.hpp"
#include <iostream>

using namespace std;

uint32_t U2F_Msg_CMD::getLe(const uint32_t byteCount, vector<uint8_t> bytes)
{
	if (byteCount > 3)
		throw runtime_error{ "Too much data for command" };
	if (byteCount != 0)
	{
		//Le must be length of data in bytes

		switch (byteCount)
		{
			case 1:
				return (bytes[0] == 0 ? 256 : bytes[0]);
			case 2:
				//Don't handle non-compliance with spec here
				//This case is only possible if extended Lc used
				//CBA
				return (bytes[0] == 0 && bytes[1] == 0 ? 65536 : bytes[0] << 8 + bytes[1]);
			case 3:
				//Don't handle non-compliance with spec here
				//This case is only possible if extended Lc not used
				//CBA
				if (bytes[0] != 0)
					throw runtime_error{ "First byte of 3-byte Le should be 0"};
				return (bytes[1] == 0 & bytes[2] == 0 ? 65536 : bytes[1] << 8 + bytes[2]);
		}
	}
	else
		return 0;
}

shared_ptr<U2F_Msg_CMD> U2F_Msg_CMD::get()
{
	const auto message = U2FMessage::read();
	
	if (message.cmd != U2FHID_MSG)
		throw runtime_error{ "Failed to get U2F Msg message" };

	U2F_Msg_CMD cmd{};
	auto &dat = message.data;

	cmd.cla = dat[0];
	cmd.ins = dat[1];
	cmd.p1  = dat[2];
	cmd.p2  = dat[3];

	const uint32_t cBCount = dat.size() - 4;

	vector<uint8_t> data{ dat.begin() + 4, dat.end() };
	auto startPtr = data.begin(), endPtr = data.end();

	if (usesData.at(cmd.ins))
	{
		clog << "Assuming command uses data" << endl;
		clog << "First bytes are: " << static_cast<uint16_t>(data[0]) << " " << static_cast<uint16_t>(data[1]) << " " << static_cast<uint16_t>(data[2]) << endl;

		if (cBCount == 0)
			throw runtime_error{ "Invalid command - should have attached data" };

		if (data[0] != 0) //1 byte length
		{
			cmd.lc = data[0];
			startPtr++;
		}
		else
		{
			cmd.lc = (data[1] << 8) + data[2];
			startPtr += 3;
		}

		clog << "Got command uses " << cmd.lc << " bytes of data" << endl;

		endPtr = startPtr + cmd.lc;

		cmd.le = getLe(data.end() - endPtr, vector<uint8_t>(endPtr, data.end()));
	}
	else
	{
		clog << "Assuming command does not use data" << endl;
		cmd.lc = 0;
		endPtr = startPtr;
		cmd.le = getLe(cBCount, data);
	}

	const auto dBytes = vector<uint8_t>(startPtr, endPtr);

	if (cmd.ins == APDU::U2F_REG)
		return make_shared<U2F_Register_APDU>(cmd, dBytes);
}

void U2F_Msg_CMD::respond(){};

const map<uint8_t, bool> U2F_Msg_CMD::usesData = {
	{ U2F_REG,  true  },
	{ U2F_AUTH, true  },
	{ U2F_VER,  false }
};
