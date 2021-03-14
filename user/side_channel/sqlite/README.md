# SQLite Side Channel

## Prepare data

Download NPPES NPI data set

``` shell
$ cd user/side_channel/data/nppes_npi
$ ./prepare.sh
```

Load NPPES data to SQLite

``` shell
$ cd user/side_channel/sqlite
$ python3 nppes_db.py gen-sqlite-basic
```
