#! /bin/bash

data_file="NPPES_Data_Dissemination_May_2022.zip"
data_url="https://download.cms.gov/nppes/${data_file}"

function download_and_prepare() {
	wget -c "${data_url}"
	unzip ${data_file}
}

if [ -f "${data_file}" ]; then
	echo "File [${data_file}] exists, skip preparing"
else
	download_and_prepare
fi
