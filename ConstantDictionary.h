#pragma once

class ConstantDictionary
{
public:
	ConstantDictionary();
	~ConstantDictionary();

	static const char* BkMode(int);
	static const char* ClipRgnMergeMode(int);
	static const char* ColorTableUsage(int);
	static const char* ICMMode(int);
	static const char* Layout(int);
	static const char* MapMode(int);
	static const char* PenStyle(int);
	static const char* PolyFillMode(int);
	static const char* ROP2(int);
	static const char* ROP3(int);
	static const char* StockObject(int);
	static const char* StretchBltMode(int);
	static const char* TextAlign(int);
	static const char* WorldTransform(int);

	static const char* EmfPlusRecordType(int);
};

