function exec(cmd, args){
	switch(cmd){
		case 'INIT':
			return cmd + args + ' <test>';
		default :
			return cmd + ':' + args + '/n';
	}
}
