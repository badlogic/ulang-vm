
exports.up = async (knex) => {
	return knex.schema.raw(`
		CREATE TABLE hashes (
			hash varchar(255) NOT NULL,
			user varchar(255) NOT NULL,
			PRIMARY KEY (hash)
		) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
	`)
}

exports.down = async (knex) => {
	knex.schema.dropTable("hashes");
}

