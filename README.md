# EmfParser
Parse Windows EMF into GDI calls.

Output of the metafile example.emf:
```cpp
//ENHMETAHEADER
//rclBounds=(20,20,920,480) Inclusive-inclusive bounds in device units
//rclFrame=(543,543,24965,13022) Inclusive-inclusive Picture Frame of metafile in .01 mm units
//nVersion=65536
//nBytes=500 Size of the metafile in bytes
//nRecords=15 Number of records in the metafile
//nHandles=1 Number of handles in the handle table. Handle index zero is reserved.
//szlDevice=(1920, 1080) Size of the reference device in pixels
//szlMillimeters=(521, 293) Size of the reference device in millimeters
//Hasn't OpenGL commands

HGDIOBJ gdiHandles[1] = { 0 };
HGDIOBJ g_stockObject = NULL;

//End of EmfRecordTypeHeader
{
	XFORM xf = { 2, 0, 0, 2, 0, 0 };
	SetWorldTransform(hdc, &xf);
}
//End of EmfRecordTypeSetWorldTransform
{
	XFORM xf = { 2, 0, 0, 2, 0, 0 };
	ModifyWorldTransform(hdc, &xf, 4);
}
//End of EmfRecordTypeModifyWorldTransform
MoveToEx(hdc, 10, 10, nullptr);
//End of EmfRecordTypeMoveToEx
LineTo(hdc, 100, 100);
//End of EmfRecordTypeLineTo
MoveToEx(hdc, 100, 10, nullptr);
//End of EmfRecordTypeMoveToEx
LineTo(hdc, 10, 100);
//End of EmfRecordTypeLineTo
SetMapMode(hdc, MM_ANISOTROPIC);
//End of EmfRecordTypeSetMapMode
SetWindowExtEx(hdc, 1, 1, nullptr);
//End of EmfRecordTypeSetWindowExtEx
SetViewportExtEx(hdc, 2, 2, nullptr);
//End of EmfRecordTypeSetViewportExtEx
g_stockObject = GetStockObject(NULL_BRUSH);
SelectObject(hdc, g_stockObject);
//End of EmfRecordTypeSelectObject
Ellipse(hdc, 10, 10, 100, 100);
//End of EmfRecordTypeEllipse
{
	struct tagPOINT points = { // sizeof(struct tagPOINT) = 8
		{ 20,50 },{ 180,50 },{ 180,20 },{ 230,70 },{ 180,120 },{ 180,90 },{ 20,90 }
	};
	Polyline(hdc, points, 7);
}
//End of EmfRecordTypePolyline16
{
	unsigned int polys = { // sizeof(unsigned int) = 4
		4,4,7
	};
	struct tagPOINT points = { // sizeof(struct tagPOINT) = 8
		{ 50,20 },{ 20,60 },{ 80,60 },{ 50,20 },{ 70,20 },{ 100,60 },{ 130,20 },{ 70,20 },{ 145,20 },{ 130,40 },{ 145,60 },{ 165,60 },{ 180,40 },{ 165,20 },{ 145,20 }
	};
	PolyPolyline(hdc, points, polys, 3);
}
//End of EmfRecordTypePolyPolyline16
// EOF
//End of EmfRecordTypeEOF
```
