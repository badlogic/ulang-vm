exports.up = function (knex) {
	return knex.schema.raw(`
	CREATE TABLE projects (
		user varchar(255) NOT NULL,
		title varchar(255) NOT NULL,
		gistid varchar(255) NOT NULL,
		created timestamp NOT NULL DEFAULT current_timestamp(),
		modified timestamp NOT NULL DEFAULT current_timestamp() ON UPDATE current_timestamp(),
		PRIMARY KEY (gistid),
		INDEX (user)
	  ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
	`)
};

exports.down = function (knex) {
	return knex.schema.dropTable("projects");
};
