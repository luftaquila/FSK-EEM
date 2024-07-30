import fs from 'fs'
import express from 'express'
import { Server } from 'socket.io'
import dateFormat from 'dateformat'
import winston from 'winston';
import winstonDaily from 'winston-daily-rotate-file';

/******************************************************************************
 * server configurations
 *****************************************************************************/
const config = JSON.parse(fs.readFileSync('config.json'));

/*****************************************************************************
 * logger configurations
 ****************************************************************************/
const logger = winston.createLogger({
  transports: [
    new winstonDaily({
      level: 'info',
      datePattern: 'YYYY-MM-DD',
      dirname: './log',
      filename: `%DATE%.log`,
    })
  ],
  format: winston.format.combine(
    winston.format.timestamp({
      format: 'YYYY-MM-DD HH:mm:ss.SSS'
    }),
    winston.format.json()
  ),
  exitOnError: false,
});

/*****************************************************************************
 * socket server configurations
 ****************************************************************************/
const io = new Server(config.port);
logger.info('SERVER STARTUP', { port: config.port });

/*****************************************************************************
 * socket connection request handler
 ****************************************************************************/
io.sockets.on('connection', socket => {
  // relay device request
  if (socket.handshake.query.relay) {
    register_relay(socket);
  }

  else {
    const client = config.clients.find(x => x.name === socket.handshake.query.client);

    if (client && (client.key === socket.handshake.query.key)) {
      register_client(socket);
    }

    else {
      socket.disconnect();

      logger.error('UNAUTHORIZED CLIENT', {
        id: socket.id,
        ip: socket.handshake.headers['x-forwarded-for'],
        client: socket.handshake.query.client,
        key: socket.handshake.query.key
      });

      return;
    }
  }
});

/*****************************************************************************
 * socket register and event handlers
 ****************************************************************************/
function register_relay(socket) {
  socket.join(socket.handshake.query.relay);

  logger.info('RELAY CONNECTED', {
    id: socket.id,
    ip: socket.handshake.headers['x-forwarded-for'],
  });

  socket.on('disconnect', reason => {
    socket.broadcast.to(socket.handshake.query.client).emit('socket-lost', { data: reason });

    logger.info('RELAY DISCONNECTED', {
      id: socket.id,
      ip: socket.handshake.headers['x-forwarded-for'],
    });
  });

  // socket event handler
}

function register_client(socket) {
  socket.join(socket.handshake.query.channel);

  logger.info('CLIENT CONNECTED', {
    id: socket.id,
    ip: socket.handshake.headers['x-forwarded-for'],
    name: socket.handshake.query.client,
  });

  // socket event handler
}
