-- SELECT COUNT(*) FROM npi_data WHERE lower(city)='houston';
-- SELECT COUNT(*) FROM npi_data WHERE name like 'ZW%' LIMIT 100;
-- select * from basic, location where basic.npi = location.npi and basic.npi == 1497758544;
-- select * from basic where basic.npi == 1497758544;
update basic set name = "Updated Name 5" where npi == 1144223363;
