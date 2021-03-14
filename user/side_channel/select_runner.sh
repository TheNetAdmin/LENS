# To be sourced

readarray -d '' runner_array < <(find ./runners/ -maxdepth 1 -name "*.sh" -print0 | sort -V -z)
if [ ${#runner_array[@]} -eq 0 ]; then
	echo "Did not find any runner script"
	exit 1
fi

echo "Select the runner script"
select runner_script in "${runner_array[@]}"; do
	source ${runner_script}
	export runner_script
	break
done
