#include "SchemaMeta.hpp"

void SchemaMeta::print_keyspace(const char* keyspace)
{
	const CassSchema* schema = cass_session_get_schema(m_session);
	const CassSchemaMeta* keyspace_meta = cass_schema_get_keyspace(schema, keyspace);

	if (keyspace_meta != NULL)
		print_schema_meta(keyspace_meta, 0);
	else
		fprintf(stderr, "Unable to find \"%s\" keyspace in the schema metadata\n", keyspace);

	cass_schema_free(schema);
}

void SchemaMeta::print_table(const char* keyspace, const char* table)
{
	const CassSchema* schema = cass_session_get_schema(m_session);
	const CassSchemaMeta* keyspace_meta = cass_schema_get_keyspace(schema, keyspace);

	if (keyspace_meta != NULL)
	{
		const CassSchemaMeta* table_meta = cass_schema_meta_get_entry(keyspace_meta, table);
		if (table_meta != NULL)
			print_schema_meta(table_meta, 0);
		else
			fprintf(stderr, "Unable to find \"%s\" table in the schema metadata\n", table);
	}
	else
		fprintf(stderr, "Unable to find \"%s\" keyspace in the schema metadata\n", keyspace);

	cass_schema_free(schema);
}

void SchemaMeta::_run()
{
	//drop_keyspace_if_not_exists();

	execute_query("CREATE KEYSPACE examples WITH replication = { \
			      'class': 'SimpleStrategy', 'replication_factor': '3' };");

	print_keyspace("examples");

	execute_query("CREATE TABLE examples.schema_meta (key text, \
	               value bigint, \
		           PRIMARY KEY (key));");

	print_table("examples", "schema_meta");
}

void SchemaMeta::print_indent(int indent)
{
	int i;
	for (i = 0; i < indent; ++i)
		printf("\t");
}

void SchemaMeta::print_schema_value(const CassValue* value)
{
	cass_int32_t i;
	cass_bool_t b;
	cass_double_t d;
	const char* s;
	size_t s_length;
	CassUuid u;
	char us[CASS_UUID_STRING_LENGTH];

	CassValueType type = cass_value_type(value);
	switch (type)
	{
	case CASS_VALUE_TYPE_INT:
		cass_value_get_int32(value, &i);
		printf("%d", i);
		break;

	case CASS_VALUE_TYPE_BOOLEAN:
		cass_value_get_bool(value, &b);
		printf("%s", b ? "true" : "false");
		break;

	case CASS_VALUE_TYPE_DOUBLE:
		cass_value_get_double(value, &d);
		printf("%f", d);
		break;

	case CASS_VALUE_TYPE_TEXT:
	case CASS_VALUE_TYPE_ASCII:
	case CASS_VALUE_TYPE_VARCHAR:
		cass_value_get_string(value, &s, &s_length);
		printf("\"%.*s\"", (int)s_length, s);
		break;

	case CASS_VALUE_TYPE_UUID:
		cass_value_get_uuid(value, &u);
		cass_uuid_string(u, us);
		printf("%s", us);
		break;

	case CASS_VALUE_TYPE_LIST:
		print_schema_list(value);
		break;

	case CASS_VALUE_TYPE_MAP:
		print_schema_map(value);
		break;

	default:
		if (cass_value_is_null(value))
			printf("null");
		else
			printf("<unhandled type>");
		break;
	}
}

void SchemaMeta::print_schema_list(const CassValue* value)
{
	CassIterator* iterator = cass_iterator_from_collection(value);
	cass_bool_t is_first = cass_true;

	printf("[ ");
	while (cass_iterator_next(iterator))
	{
		if (!is_first)
			printf(", ");
		print_schema_value(cass_iterator_get_value(iterator));
		is_first = cass_false;
	}
	printf(" ]");
	cass_iterator_free(iterator);
}

void SchemaMeta::print_schema_map(const CassValue* value)
{
	CassIterator* iterator = cass_iterator_from_map(value);
	cass_bool_t is_first = cass_true;

	printf("{ ");
	while (cass_iterator_next(iterator))
	{
		if (!is_first)
			printf(", ");
		print_schema_value(cass_iterator_get_map_key(iterator));
		printf(" : ");
		print_schema_value(cass_iterator_get_map_value(iterator));
		is_first = cass_false;
	}
	printf(" }");
	cass_iterator_free(iterator);
}

void SchemaMeta::print_schema_meta_field(const CassSchemaMetaField* field, int indent)
{
	const char* name;
	size_t name_length;
	const CassValue* value;

	cass_schema_meta_field_name(field, &name, &name_length);
	value = cass_schema_meta_field_value(field);

	print_indent(indent);
	printf("%.*s: ", (int)name_length, name);
	print_schema_value(value);
	printf("\n");
}

void SchemaMeta::print_schema_meta_fields(const CassSchemaMeta* meta, int indent)
{
	CassIterator* fields = cass_iterator_fields_from_schema_meta(meta);

	while (cass_iterator_next(fields))
		print_schema_meta_field(cass_iterator_get_schema_meta_field(fields), indent);

	cass_iterator_free(fields);
}

void SchemaMeta::print_schema_meta_entries(const CassSchemaMeta* meta, int indent)
{
	CassIterator* entries = cass_iterator_from_schema_meta(meta);

	while (cass_iterator_next(entries))
		print_schema_meta(cass_iterator_get_schema_meta(entries), indent);

	cass_iterator_free(entries);
}

void SchemaMeta::print_schema_meta(const CassSchemaMeta* meta, int indent)
{
	const CassSchemaMetaField* field = NULL;
	const char* name;
	size_t name_length;
	print_indent(indent);

	switch (cass_schema_meta_type(meta))
	{
	case CASS_SCHEMA_META_TYPE_KEYSPACE:
		field = cass_schema_meta_get_field(meta, "keyspace_name");
		cass_value_get_string(cass_schema_meta_field_value(field), &name, &name_length);
		printf("Keyspace \"%.*s\":\n", (int)name_length, name);
		print_schema_meta_fields(meta, indent + 1);
		printf("\n");
		print_schema_meta_entries(meta, indent + 1);
		break;

	case CASS_SCHEMA_META_TYPE_TABLE:
		field = cass_schema_meta_get_field(meta, "columnfamily_name");
		cass_value_get_string(cass_schema_meta_field_value(field), &name, &name_length);
		printf("Table \"%.*s\":\n", (int)name_length, name);
		print_schema_meta_fields(meta, indent + 1);
		printf("\n");
		print_schema_meta_entries(meta, indent + 1);
		break;

	case CASS_SCHEMA_META_TYPE_COLUMN:
		field = cass_schema_meta_get_field(meta, "column_name");
		cass_value_get_string(cass_schema_meta_field_value(field), &name, &name_length);
		printf("Column \"%.*s\":\n", (int)name_length, name);
		print_schema_meta_fields(meta, indent + 1);
		printf("\n");
		break;
	}
}
