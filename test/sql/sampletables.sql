-- This file creates some tables to be used in the tests

CREATE TABLE testtab1 (id integer, t varchar(20));
INSERT INTO testtab1 VALUES (1, 'foo');
INSERT INTO testtab1 VALUES (2, 'bar');
INSERT INTO testtab1 VALUES (3, 'foobar');

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
