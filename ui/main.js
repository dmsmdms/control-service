'use strict'

const WS_MSG_PROC_INFO = 1;

const WS_CMD_BIND_PROC_MONITOR = 1;
const WS_CMD_UNBIND_PROC_MONITOR = 2;
const WS_CMD_GET_PROC_INFO = 3;
const WS_CMD_CONTINUE_PROC = 4;
const WS_CMD_PAUSE_PROC = 5;
const WS_CMD_STOP_PROC = 6;
const WS_CMD_KILL_PROC = 7;

function fillListTbody(tbody, name, socket, list) {
	while (tbody.firstElementChild != undefined) {
		tbody.firstElementChild.remove();
	}

	for (let i = 0; i < list.length; i++) {
		const tr = document.createElement('tr');
		const nameTd = document.createElement('td');
		const pidTd = document.createElement('td');
		const virtMemTd = document.createElement('td');
		const phyMemTd = document.createElement('td');
		const shMemTd = document.createElement('td');
		const statusTd = document.createElement('td');
		const cpuTd = document.createElement('td');
		const memTd = document.createElement('td');
		const timeTd = document.createElement('td');
		const commandsTd = document.createElement('td');
		const runBtn = document.createElement('button');
		const pauseBtn = document.createElement('button');
		const stopBtn = document.createElement('button');
		const killBtn = document.createElement('button');
		const item = list[i];

		tbody.appendChild(tr);
		tr.appendChild(nameTd);
		tr.appendChild(pidTd);
		tr.appendChild(virtMemTd);
		tr.appendChild(phyMemTd);
		tr.appendChild(shMemTd);
		tr.appendChild(statusTd);
		tr.appendChild(cpuTd);
		tr.appendChild(memTd);
		tr.appendChild(timeTd);
		tr.appendChild(commandsTd);
		commandsTd.appendChild(runBtn);
		commandsTd.appendChild(pauseBtn);
		commandsTd.appendChild(stopBtn);
		commandsTd.appendChild(killBtn);

		runBtn.onclick = function () {
			sendCmd(socket, [{ code: WS_CMD_CONTINUE_PROC, pid: item.pid }]);
		};

		pauseBtn.onclick = function () {
			sendCmd(socket, [{ code: WS_CMD_PAUSE_PROC, pid: item.pid }]);
		};

		stopBtn.onclick = function () {
			sendCmd(socket, [{ code: WS_CMD_STOP_PROC, pid: item.pid }]);
		};

		killBtn.onclick = function () {
			sendCmd(socket, [{ code: WS_CMD_KILL_PROC, pid: item.pid }]);
		};

		if (name.length > 0) {
			const nameHightLight = `<span style="color: red">${ name }</span>`;
			nameTd.innerHTML = item.name.replace(name, nameHightLight);
		} else {
			nameTd.innerHTML = item.name;
		}

		pidTd.innerText = item.pid;
		virtMemTd.innerText = item.virtMem;
		phyMemTd.innerText = item.phyMem;
		shMemTd.innerText = item.shMem;
		statusTd.innerText = item.status;
		cpuTd.innerText = item.cpu;
		memTd.innerText = item.mem;
		timeTd.innerText = item.time;
		runBtn.innerText = 'Run';
		pauseBtn.innerText = 'Pause';
		stopBtn.innerText = 'Stop';
		killBtn.innerText = 'Kill';
	}
}

function getStatusStr(statusCode) {
	const statusStr = [
		'Sleep',
	];
	
	return statusStr[statusCode];
}

function decodeInfo(blob) {
	const decoder = new TextDecoder('utf-8');
	const data = new DataView(blob);
	const list = [];
	let offset = 0;
	
	while (offset < data.byteLength) {
		const type = data.getUint8(offset);
		offset += 1;
		
		switch (type) {
		case WS_MSG_PROC_INFO:
			const length = data.getUint8(offset);
			offset += 1;
			
			for (let i = 0; i < length; i++) {
				const pid = data.getUint32(offset);
				offset += 4;
				
				const virtMem = data.getUint32(offset);
				offset += 4;
				
				const phyMem = data.getUint32(offset);
				offset += 4;
				
				const shMem = data.getUint32(offset);
				offset += 4;
				
				const time = data.getUint32(offset);
				offset += 4;
				
				const cpu = data.getUint16(offset);
				offset += 2;
				
				const mem = data.getUint16(offset);
				offset += 2;
				
				const statusCode = data.getUint8(offset);
				const status = getStatusStr(statusCode);
				offset += 1;
				
				const nameLen = data.getUint8(offset);
				offset += 1;
				
				const nameArr = new Uint8Array(blob, offset, nameLen);
				const name = decoder.decode(nameArr);
				offset += nameLen;
				
				const item = {
					name: name,
					pid: pid,
					virtMem: virtMem,
					phyMem: phyMem,
					shMem: shMem,
					status: status,
					cpu: cpu,
					mem: mem,
					time: time,
				};
				
				list.push(item);
			}
			
			break;
		}
	}
	
	const info = {
		list: list,
	};

	return info;
}

function sendCmd(socket, cmd_list) {
	const encoder = new TextEncoder('utf-8');
	const response = [];
	
	for (let i = 0; i < cmd_list.length; i++) {
		const cmd = cmd_list[i];
		response.push(cmd.code);
		
		if (cmd.str != undefined) {
			const cstr = encoder.encode(cmd.str);
			response.push(cstr.length);
			
			for (let j = 0; j < cstr.length; j++) {
				response.push(cstr[j]);
			}
		}
		
		if (cmd.pid != undefined) {
			// Not what you thought :D
			const pid_arr = new Uint32Array(1);
			pid_arr[0] = cmd.pid;
			
			const pid_str = new Uint8Array(pid_arr.buffer);
			for (let j = 0; j < pid_str.length; j++) {
				response.push(pid_str[j]);
			}
		}
	}
	
	const blob = new Uint8Array(response.length);
	for (let i = 0; i < response.length; i++) {
		blob[i] = response[i];
	}

	socket.binaryType = 'arraybuffer';
	socket.send(blob.buffer);
}

onload = function () {
	const socket = 	new WebSocket(`ws://${ location.host }/`);
	const listTbody = document.getElementById('list-tbody');
	const procName = document.getElementById('proc-name');

	socket.onopen = function() {
		procName.oninput = function () {
			const cmd_list = [
				{ code: WS_CMD_GET_PROC_INFO, str: this.value },
			];

			sendCmd(socket, cmd_list);
		};

		const cmd_list = [
			{ code: WS_CMD_BIND_PROC_MONITOR },
		];

		sendCmd(socket, cmd_list);
	};

	socket.onmessage = function(event) {
		const info = decodeInfo(event.data);
		fillListTbody(listTbody, procName.value, socket, info.list);
	};
};
