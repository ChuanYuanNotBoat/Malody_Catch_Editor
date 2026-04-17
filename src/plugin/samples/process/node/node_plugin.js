const readline = require('readline');

const rl = readline.createInterface({
  input: process.stdin,
  output: process.stdout,
  terminal: false,
});

function send(obj) {
  process.stdout.write(JSON.stringify(obj) + "\n");
}

rl.on('line', (line) => {
  if (!line || !line.trim()) {
    return;
  }

  let msg;
  try {
    msg = JSON.parse(line);
  } catch {
    return;
  }

  if (msg.type === 'notify') {
    if (msg.event === 'shutdown') {
      process.exit(0);
    }
    return;
  }

  if (msg.type === 'request') {
    const id = msg.id || '';
    send({ type: 'response', id, result: false });
  }
});
