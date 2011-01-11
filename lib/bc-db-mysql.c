/*
 * Copyright (C) 2010 Bluecherry, LLC
 *
 * Confidential, all rights reserved. No distribution is permitted.
 */

#include <stdlib.h>
#include <string.h>

#include <libbluecherry.h>

#include <mysql/mysql.h>

struct bc_db_mysql_res {
	MYSQL_RES *res;
	MYSQL_ROW row;
	int ncols;
};	

static void *bc_db_mysql_open(struct config_t *cfg)
{
	const char *dbname, *user, *password;
	const char *host = NULL;
	const char *socket = NULL;
	long port = 0;
	MYSQL *mysql;

        if (!config_lookup_string(cfg, BC_CONFIG_DB ".dbname", &dbname))
		return NULL;
	if (!config_lookup_string(cfg, BC_CONFIG_DB ".user", &user))
		return NULL;
	if (!config_lookup_string(cfg, BC_CONFIG_DB ".password", &password))
		return NULL;

	if (config_lookup_string(cfg, BC_CONFIG_DB ".host", &host))
		config_lookup_int(cfg, BC_CONFIG_DB ".port", &port);
	else
		config_lookup_string(cfg, BC_CONFIG_DB ".socket", &socket);

	if ((mysql = mysql_init(NULL)) == NULL) {
		bc_log("(SQL ERROR): Initializing MySQL");
		return NULL;
	}

	if (mysql_real_connect(mysql, host, user, password, dbname,
			       (int)port, socket, 0) == NULL) {
		mysql_close(mysql);
		bc_log("(SQL ERROR): Connecting to MySQL database");
		return NULL;
	}

	return mysql;
}

static void bc_db_mysql_close(void *handle)
{
	mysql_close(handle);
}

static int bc_db_mysql_query(void *handle, char *query)
{
	int ret = mysql_query(handle, query);

	if (ret != 0)
		bc_log("(SQL ERROR): '%s' => %d", query, ret);
		
	return ret;
}

static BC_DB_RES bc_db_mysql_get_table(void *handle, char *query)
{
	struct bc_db_mysql_res *dbres = malloc(sizeof(*dbres));
	int ret;

	if (dbres == NULL)
		return NULL;

	ret = mysql_query(handle, query);

	if (ret != 0) {
		free(dbres);
		bc_log("(SQL ERROR): '%s' => %d", query, ret);
		return NULL;
	}

	dbres->res = mysql_store_result(handle);
	if (dbres->res == NULL) {
		free(dbres);
		bc_log("(SQL ERROR): No result: '%s'", query);
		return NULL;
	}

	dbres->ncols = mysql_num_fields(dbres->res);

	return dbres;
}

static void bc_db_mysql_free_table(void *handle, BC_DB_RES __dbres)
{
	struct bc_db_mysql_res *dbres = __dbres;

	mysql_free_result(dbres->res);
}

static int bc_db_mysql_fetch_row(void *handle, BC_DB_RES __dbres)
{
	struct bc_db_mysql_res *dbres = __dbres;

	dbres->row = mysql_fetch_row(dbres->res);
	if (dbres->row == NULL) {
		mysql_data_seek(dbres->res, 0);
		return -1;
	}

	return 0;
}

static const char *bc_db_mysql_get_val(void *handle, BC_DB_RES __dbres,
				       const char *colname)
{
	struct bc_db_mysql_res *dbres = __dbres;
	int i;

	if (dbres->row == NULL)
		return NULL;

	for (i = 0; i < dbres->ncols; i++) {
		MYSQL_FIELD *mf = mysql_fetch_field_direct(dbres->res, i);
		if (mf == NULL)
			continue;
		if (strcmp(colname, mf->name) == 0)
			break;
	}

	return (i == dbres->ncols) ? NULL : dbres->row[i];
}

static int bc_db_mysql_num_fields(void *handle, BC_DB_RES __dbres)
{
	struct bc_db_mysql_res *dbres = __dbres;

	return dbres->ncols;
}

static const char *bc_db_mysql_get_field(void *handle, BC_DB_RES __dbres,
					 int nfield)
{
	struct bc_db_mysql_res *dbres = __dbres;
	MYSQL_FIELD *fields = mysql_fetch_fields(dbres->res);

	if (!fields || nfield < 0 || nfield >= dbres->ncols)
		return NULL;

	return fields[nfield].name;
}

static unsigned long bc_db_mysql_last_insert_rowid(void *handle)
{
	return mysql_insert_id(handle);
}

static void bc_db_mysql_escape_string(void *handle, char *to,
				      const char *from)
{
	mysql_real_escape_string(handle, to, from, strlen(from));
}

static void bc_db_mysql_lock(void *handle, const char *table)
{
	bc_db_query("LOCK TABLES %s WRITE", table);
}

static void bc_db_mysql_unlock(void *handle, const char *table)
{
	bc_db_query("UNLOCK TABLES");
}

struct bc_db_ops bc_db_mysql = {
	.type		= BC_DB_SQLITE,
	.open		= bc_db_mysql_open,
	.close		= bc_db_mysql_close,
	.get_table	= bc_db_mysql_get_table,
	.free_table	= bc_db_mysql_free_table,
	.fetch_row	= bc_db_mysql_fetch_row,
	.get_val	= bc_db_mysql_get_val,
	.query		= bc_db_mysql_query,
	.num_fields	= bc_db_mysql_num_fields,
	.get_field	= bc_db_mysql_get_field,
	.last_insert_rowid = bc_db_mysql_last_insert_rowid,
	.escape_string	= bc_db_mysql_escape_string,
	.lock		= bc_db_mysql_lock,
	.unlock		= bc_db_mysql_unlock,
};
