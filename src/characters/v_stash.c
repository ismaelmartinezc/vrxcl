#include "../g_local.h"
#include "io/v_characterio.h"

stash_io_t vrx_stash_io = {0};

/*
 *
 * BASIC IO
 *
 */

#ifndef NO_GDS
qboolean mysql_stash_store(edict_t* ent, int itemindex)
{
	gds_queue_add(ent, GDS_STASH_STORE, itemindex);
	return true;
}

qboolean mysql_stash_get_page(edict_t* ent, int page, int numitems)
{
	gds_queue_add(ent, GDS_STASH_GET_PAGE, page);
	return true;
}

qboolean mysql_stash_open(edict_t* ent)
{
	gds_queue_add(ent, GDS_STASH_OPEN, -1);
	return true;
}

qboolean mysql_stash_close(edict_t* ent)
{
	gds_queue_add(ent, GDS_STASH_CLOSE, -1);
	return true;
}

qboolean mysql_stash_take(edict_t* ent, int stash_index)
{
	gds_queue_add(ent, GDS_STASH_TAKE, stash_index);
	return true;
}

qboolean mysql_stash_close_id(int owner_id)
{
	gds_queue_add(NULL, GDS_STASH_CLOSE, owner_id);
	return true;
}

void vrx_setup_mysql_stash()
{
	vrx_stash_io = (stash_io_t){
		.open_stash = mysql_stash_open,
		.close_stash = mysql_stash_close,
		.close_stash_by_id = mysql_stash_close_id,
		.take = mysql_stash_take,
		.get_page = mysql_stash_get_page,
		.store = mysql_stash_store
	};
}
#endif


void vrx_setup_sqlite_stash()
{
	vrx_stash_io = (stash_io_t){
		0
	};
}

void vrx_init_stash_io()
{
	int method = savemethod->value;
	switch (method) {
#ifndef NO_GDS
	case SAVEMETHOD_MYSQL:
		vrx_setup_mysql_stash();
		break;
#endif
	default:
		gi.dprintf("stash io: unsupported method, defaulting to 3 (sqlite single file mode)");
	case SAVEMETHOD_SQLITE:
		vrx_setup_sqlite_stash();
		break;
	}
}

void vrx_close_stash_io()
{

}

void Cmd_Stash_f(edict_t* ent)
{
	vrx_stash_open(ent);
}

void vrx_stash_store(edict_t* ent, int itemindex)
{
	vrx_stash_io.store(ent, itemindex);
}

void vrx_notify_stash_no_owner(void* args)
{
	stash_event_t* notif = args;

	if (notif->ent->gds_connection_id == notif->gds_connection_id && notif->ent->inuse)
		gi.cprintf(notif->ent, PRINT_HIGH,
			"Sorry, you need to set an owner or your master password in order to access the stash.\n"
		);

	V_Free(args);
}

void vrx_notify_stash_taken(void* args)
{
	stash_taken_event_t* evt = args;

	if (evt->ent->gds_connection_id != evt->gds_connection_id)
	{
		// TODO: put item back in stash
		return;
	}

	item_t* slot = V_FindFreeItemSlot(evt->ent);

	if (slot) {
		gi.sound(evt->ent, CHAN_ITEM, gi.soundindex("misc/amulet.wav"), 1, ATTN_NORM, 0);
		V_ItemCopy(&evt->taken, slot);
	}
	else {
		// TODO: put item back in stash
	}

	V_Free(args);
}

void vrx_notify_stash_locked(void* args)
{
	stash_event_t* notif = args;

	if (notif->ent->gds_connection_id == notif->gds_connection_id && notif->ent->inuse)
		gi.cprintf(notif->ent, PRINT_HIGH,
			"Sorry, your stash is currently in use.\n"
			"Close it in your other characters to become able to use it again,\n"
			"or contact an administrator if you're having issues."
		);

	V_Free(args);
}

void vrx_notify_open_stash(void* args)
{
	stash_page_event_t* notif = args;

	if (notif->ent->gds_connection_id != notif->gds_connection_id || !notif->ent->inuse)
	{
		if (vrx_stash_io.close_stash_by_id)
			vrx_stash_io.close_stash_by_id(notif->gds_owner_id);

		V_Free(args);
		return;
	}

	vrx_stash_open_page(notif->ent, notif->page, sizeof notif->page / sizeof(item_t), notif->pagenum);
	V_Free(args);
}

/*
 *
 * MENU STUFF
 *
 */

void handler_stash_item(edict_t* ent, int option)
{
	if (option == 5000) {
		// do not close the stash
		ent->client->menustorage.cancel_close_event = true;
		return;
	}

	vrx_stash_io.take(ent, option - 1000);
}

void vrx_show_stash_item(edict_t* ent, item_t* item, int stash_index)
{
	if (!menu_can_show(ent))
		return;

	menu_clear(ent);

	menu_add_line(ent, V_MenuItemString(item, ' '), MENU_GREEN_LEFT);

	StartShowInventoryMenu(ent, item);

	menu_add_line(ent, "", 0);
	menu_add_line(ent, "Take", 1000 + stash_index);
	menu_add_line(ent, "Back", 5000);

	menu_set_handler(ent, handler_stash_item);
	menu_set_close_handler(ent, vrx_stash_close); // ran after selection or if menu is unexpectedly closed

	ent->client->menustorage.currentline = ent->client->menustorage.num_of_lines - 1;

	menu_show(ent);
}

void handler_stash_page(edict_t* ent, int opt)
{
	if (opt >= 2000)
	{
		// next pg
		ent->client->menustorage.cancel_close_event = true;
		vrx_stash_io.get_page(ent, opt - 2000, sizeof ent->client->stash.page / sizeof(item_t));
		return;
	}

	if (opt == 1500)
	{
		// exit
		return;
	}

	if (opt >= 1000)
	{
		// prev pg
		ent->client->menustorage.cancel_close_event = true;
		vrx_stash_io.get_page(ent, opt - 1000, sizeof ent->client->stash.page / sizeof(item_t));
		return;
	}

	// show stash item
	const int page_len = sizeof ent->client->stash.page / sizeof(item_t);
	int item_index = (opt - 100) % page_len;
	int page = (opt - 100) / page_len;
	item_t* it = &ent->client->stash.page[item_index];
	if (it->itemtype != ITEM_NONE)
		vrx_show_stash_item(ent, it, opt - 100);
	else
		vrx_stash_open_page(ent, ent->client->stash.page, page_len, page);

	ent->client->menustorage.cancel_close_event = true;
}

void vrx_stash_open(edict_t* ent)
{
	vrx_stash_io.open_stash(ent);
}

void vrx_stash_open_page(edict_t* ent, item_t* page, int item_count, int page_index)
{
	if (!menu_can_show(ent))
		return;
	menu_clear(ent);

	menu_add_line(ent, va("%s's stash (Page %d)", ent->client->pers.netname, page_index + 1), MENU_GREEN_CENTERED);
	menu_add_line(ent, "", 0);

	for (int i = 0; i < item_count; i++)
	{
		lva_result_t line = vrx_get_item_menu_line(&page[i]);
		int opt = 100 + page_index * 10 + i;
		menu_add_line(ent, line.str, opt);
	}

	menu_add_line(ent, "", 0);
	if (page_index < 99)
	menu_add_line(ent, "Next", 1000 + page_index + 1);

	int amt = 1;
	if (page_index > 0) {
		menu_add_line(ent, "Previous", 2000 + page_index - 1);
		amt++;
	}

	menu_add_line(ent, "Exit", 1500);

	menu_set_handler(ent, handler_stash_page);
	menu_set_close_handler(ent, vrx_stash_close);

	ent->client->menustorage.currentline = ent->client->menustorage.num_of_lines - amt;

	menu_show(ent);

	assert(item_count <= (sizeof ent->client->stash.page / sizeof(item_t)));
	if (page != ent->client->stash.page)  // xxx: hack to prevent copying to same memory
		memcpy(ent->client->stash.page, page, sizeof(item_t) * item_count);
}

void vrx_stash_close(edict_t* ent)
{
	memset(&ent->client->stash, 0, sizeof ent->client->stash);
	vrx_stash_io.close_stash(ent);
}
