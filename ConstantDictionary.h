/***************************************************************************
* Copyright (C) 2017, Deping Chen, cdp97531@sina.com
*
* All rights reserved.
* For permission requests, write to the author.
*
* This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
* KIND, either express or implied.
***************************************************************************/
#pragma once

class ConstantDictionary
{
public:
	ConstantDictionary();
	~ConstantDictionary();

	static const char* BkMode(int);
	static const char* BigBool(int);
	static const char* BrushStyle(int);
	static const char* CharSet(int);
	static const char* CharPrecision(int);
	static const char* CharQuality(int);
	static const char* ClipPrecision(int);
	static const char* ClipRgnMergeMode(int);
	static const char* ColorTableUsage(int);
	static const char* ExtTextOutOptions(int);
	static const char* FontWeight(int);
	static const char* HatchStyle(int);
	static const char* ICMMode(int);
	static const char* Layout(int);
	static const char* MapMode(int);
	static const char* PenStyle(int);
	static const char* PitchAndFamily(int);
	static const char* PolyFillMode(int);
	static const char* RGBColor(int);
	static const char* ROP2(int);
	static const char* ROP3(int);
	static const char* StockObject(int);
	static const char* StretchBltMode(int);
	static const char* TextAlign(int);
	static const char* WorldTransform(int);

	static const char* EmfPlusRecordType(int);
};

