/*
 * Acess2 Window Manager v3
 * - By John Hodge (thePowersGang)
 * 
 * renderer/widget/disptext.c
 * - Label Text
 */
#include <common.h>
#include "./common.h"
#include "./colours.h"
#include <string.h>

void Widget_DispText_Render(tWindow *Window, tElement *Element)
{
	WM_Render_DrawText(
		Window,
		Element->CachedX+1, Element->CachedY+1,
		Element->CachedW-2, Element->CachedH-2,
		NULL, TEXT_COLOUR,
		Element->Text, -1
		);
}

void Widget_DispText_UpdateText(tElement *Element, const char *Text)
{
	 int	w=0, h=0;

	if(Element->Text)	free(Element->Text);
	Element->Text = strdup(Text);

	WM_Render_GetTextDims(NULL, Element->Text, -1, &w, &h);

	// Apply edge padding
	w += 2;	h += 2;
	
	Element->MinW = w;
	Element->MinH = h;

	Widget_UpdateMinDims(Element->Parent);
}

DEFWIDGETTYPE(ELETYPE_TEXT, "Text",
	WIDGETTYPE_FLAG_NOCHILDREN,
	.Render = Widget_DispText_Render,
	.UpdateText = Widget_DispText_UpdateText
	);

