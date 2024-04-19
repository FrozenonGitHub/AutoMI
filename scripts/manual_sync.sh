IP_LIST=("172.31.14.234"
"172.31.15.108"
"172.31.4.156"
"172.31.1.63"
"172.31.2.224"
"172.31.0.195"
"172.31.6.199"
"172.31.8.233")

for i in "${!IP_LIST[@]}"; do
    ip=${IP_LIST[i]}
    scp /code/powerlyra-origin/machines ubuntu@$ip:/code/powerlyra-origin/machines
    ssh $ip
done