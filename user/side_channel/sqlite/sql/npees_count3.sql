SELECT COUNT(*) FROM basic, location WHERE basic.npi = location.npi and lower(city)='athens';
