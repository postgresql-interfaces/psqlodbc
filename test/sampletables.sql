-- This file creates some tables to be used in the tests.
--
-- Note that the reset-db program that reads this file is dumb and reads
-- it one line at a time. Hence each statement must be on a single line.

CREATE TABLE testtab1 (id integer PRIMARY KEY, t varchar(20));
INSERT INTO testtab1 VALUES (1, 'foo');
INSERT INTO testtab1 VALUES (2, 'bar');
INSERT INTO testtab1 VALUES (3, 'foobar');

CREATE TABLE testtab_fk (id integer REFERENCES testtab1, t varchar(20));
INSERT INTO testtab_fk VALUES (1, 'hoge');
INSERT INTO testtab_fk VALUES (2, 'pogno');
INSERT INTO testtab_fk VALUES (3, 'poco');

CREATE TABLE byteatab (id integer, t bytea);
INSERT INTO byteatab VALUES (1, E'\\001\\002\\003\\004\\005\\006\\007\\010'::bytea);
INSERT INTO byteatab VALUES (2, 'bar');
INSERT INTO byteatab VALUES (3, 'foobar');
INSERT INTO byteatab VALUES (4, 'foo');
INSERT INTO byteatab VALUES (5, 'barf');

CREATE TABLE intervaltable(id integer, iv interval, d varchar(100));
INSERT INTO intervaltable VALUES (1, '1 day', 'one day');
INSERT INTO intervaltable VALUES (2, '10 seconds', 'ten secs');
INSERT INTO intervaltable VALUES (3, '100 years', 'hundred years');

CREATE TABLE booltab (id integer, t varchar(5), b boolean);
INSERT INTO booltab VALUES (1, 'yeah', true);
INSERT INTO booltab VALUES (2, 'yes', true);
INSERT INTO booltab VALUES (3, 'true', true);
INSERT INTO booltab VALUES (4, 'false', false);
INSERT INTO booltab VALUES (5, 'not', false);

-- View
CREATE VIEW testview AS SELECT * FROM testtab1;

-- Materialized view
CREATE MATERIALIZED VIEW testmatview AS SELECT * FROM testtab1;

-- Foreign table
CREATE FOREIGN DATA WRAPPER testfdw;
CREATE SERVER testserver FOREIGN DATA WRAPPER testfdw;
CREATE FOREIGN TABLE testforeign (c1 int) SERVER testserver;

-- Procedure for catalog function checks
CREATE FUNCTION simple_add(in int, in int, out int) AS $$ SELECT $1 + $2; $$ LANGUAGE SQL;
-- 	Function Returning composite type
CREATE FUNCTION getfoo(int) RETURNS booltab AS $$ SELECT * FROM booltab WHERE id = $1; $$ LANGUAGE SQL;
--	Function Returning set of composite type
CREATE FUNCTION getboo(int) RETURNS SETOF booltab AS $$ SELECT * FROM booltab WHERE id = $1; $$ LANGUAGE SQL;
--	Function Returning table
CREATE FUNCTION tbl_arg(in p_f1 int) returns table(p_f2 text, p_f3 boolean) AS $$ SELECT t, b from booltab where id = p_f1; $$ LANGUAGE SQL;
--	The previous one is equivalent to using one or more OUT parameters
--	plus marking the function as returning SETOF record (or SETOF a single
--	output parameter's type, as appropriate).
CREATE FUNCTION set_of(in p_f1 int, out p_f2 text, out p_f3 boolean) returns setof record AS $$ SELECT t, b from booltab where id = p_f1; $$ LANGUAGE SQL;

-- Large object support
CREATE DOMAIN lo AS oid;
CREATE TABLE lo_test_tab (id int4, large_data lo);
