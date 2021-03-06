/**
 * @class SCDMsgCenter - https://github.com/SC-Develop/SCD_MC
 *
 * @author Ing. Salvatore Cerami - dev.salvatore.cerami@gmail.com - https://github.com/SC-Develop/
 *
 * @brief Message center: Interprocess message comunications
 *
 *        This is a part of SCD Message Center QT Class Library.
 *
 *        This is the core of a realtime messaging/commands system for exchange of inter-process messages/commands based on
 *        Tcp Socket
 *
 *        This file must be distribuited with files:
 *
 *           - msgserver.cpp,
 *           - msgserver.h,
 *           - msgserverthread.h,
 *           - msgserverthread.cpp,
 *           - msgthreadhandler.h
 *           - msgthreadhandler.cpp
 *
 *        Purpose: simple message/command exchange in interprocess communication (for example remoted application controll/monitoring)
 *
 *        Operating mode:
 *
 *          - The application routines, or application threads (sender) send a log message to message center.
 *          - The remote clients establish a tcp socket connection to message center server
 *          - The connected remote clients request to receive message from main application or selected application thread
 *          - The connected remote clients send command to application or selected application thread.
 *
 *        Message Center Client connections can be also local connection
 *
 *        Application that use message center need to implement a socket sever to allow remote inter-process communication.
 *        The socket server as been developed and is already distribuited with this file.
 *        You don't need to develop the socket sever.
 *        Is strictly recomended to use the already developed and socket server distribuited with this file.
 *
 */

#include "msgcenter.h"


/**
 * @brief SCDMsgCenter::SCDMsgCenter
 * @param parent
 */
SCDMsgCenter::SCDMsgCenter(QObject *parent) : QObject(parent)
{

}

/**
 * @brief SCDMsgCenter::addClient add client socket to message recipient list
 * @param socket
 * @return
 */
void SCDMsgCenter::addClient(int socketDescriptor)
{
   QMutexLocker locker(&mutex);

   registerClient(socketDescriptor);

   locker.unlock();
}

/**
 * @brief SCDMsgCenter::removeClient remove client socket from message recipent list
 * @param socket
 * @return
 */
void SCDMsgCenter::removeClient(int socketDescriptor)
{
   QMutexLocker locker(&mutex);

   unregisterClient(socketDescriptor);

   locker.unlock();
}

/**
 * @brief SCDMsgCenter::addSender add a new sender to sender list, before send any a message the sender must already be in the sender list
 * @param id
 * @return
 */
void SCDMsgCenter::addSender(QString sender)
{
   QMutexLocker locker(&mutex);

   registerMessageSender(sender);

   locker.unlock();
}

/**
 * @brief SCDMsgCenter::removeSender remove the sender fronm sender list
 * @param id
 * @return
 */
void SCDMsgCenter::removeSender(QString sender)
{
   QMutexLocker locker(&mutex);

   unregisterMessageSender(sender);

   locker.unlock();
}

/**
 * @brief SCDMsgCenter::clientSendCommand client send a command to message center.
 *
 * @param cmd commands recognized commands:
 *
 *     - list                   => get a list of message senders
 *     - spy <sender id>        => receive message only by sender identified by id
 *     - <cr> (carriage return) => stop realtime message receiving and show help
 *     - help                   => show help
 *     - exit                   => close client socket connection
 *     - @<command> <sender id> => sender a string to sender by message center
 *     - ping                   => message center reply pong (use for checking if application running)
 *     - <unkown command>       => return echo of command\n";
 *
 *     typing unknown commands causes echo reply from message center.
 *
 * @param client socket descriptor
 *
 * @return
 */
void SCDMsgCenter::sendCommand(QString cmd, int clientSocketDescriptor)
{
   QMutexLocker locker(&mutex);

   processCommand(cmd,clientSocketDescriptor);

   locker.unlock();
}

/**
 * @brief SCDMsgCenter::senderPostMessage post a message from sender to message center, the message will be forwarded
 *                                     realtime to all client who request to receive it from this sender.
 * @param mmessage message to send
 */
void SCDMsgCenter::postMessage(QString msg, QString sender, bool prependNewLine)
{
   QMutexLocker locker(&mutex);

   msg = sender + ": " + msg;

   if (prependNewLine)
   {
      msg.prepend(LF);
   }

   processMessage(msg,sender);

   locker.unlock();
}

/**
 * @brief SCDMsgCenter::getClient get the connected client identified by socket descriptor
 * @param socketDescriptor
 * @return
 */
SCDMsgCenter::Client SCDMsgCenter::getClient(int socketDescriptor)
{
   Client client;

   for (int n=0; n<clients.size();n++)
   {
      if (clients.at(n).socketDescriptor==socketDescriptor)
      {
         client = clients.at(n);
         client.index  = n; // set client index
         return client;
      }
   }

   client.index = -1; // client not found

   return client;
}

/**
 * @brief SCDMsgCenter::getSenderList get list of sender
 * @return
 */
QStringList SCDMsgCenter::getSenderList()
{
   return senders;
}

/**
 * @brief SCDMsgCenter::getHelpString return the menu string
 * @return
 */
QString SCDMsgCenter::getHelpString()
{
   return " Command Help:\n\n"
          "   - list                    => get a list of message senders\n"
          "   - spy <sender id>         => receive message only by sender identified by sender id\n"
          "   - <cr> (carriage return)  => stop realtime message receiving and show help\n"
          "   - help                    => show this help\n"
          "   - exit                    => close connection to message center\n"
          "   - @<sender id> <command>  => sends a string to sender by message center\n"
          // "   - @<sender id> help       => elenco dei comandi riconosciuti dal sender\n"
          // "   - @shell                  => apre un terminale shell (crea un thread speciale nell'applicazione)\n"
          "   - ping                    => message center reply pong\n"
          "   - <unkown command>        => return echo of command\n";
}

/**
 * @brief SCDMsgCenter::getPrompt return the console prompt
 * @param socketDescriptor
 * @return a null string if client not found or menu prompt
 */
QString SCDMsgCenter::getPrompt(int socketDescriptor)
{
   Client client = getClient(socketDescriptor);

   if (client.index > -1)
   {
      return "\n" + client.name + ":> ";
   }

   return "";
}

/**
 * @brief SCDMsgCenter::sendMessageToClient send message to destination socket thread must process this message (signal).
 * @param msg
 * @param socketDescriptor
 */
void SCDMsgCenter::sendMessageToClient(QString msg, int clientSocketDescriptor)
{
   emit messageToClient_signal(msg, clientSocketDescriptor); // serialize the messages to clients
}

/**
 * @brief SCDMsgCenter::message_slot receives and process a message from sender.
 *                                sends message to destination client which asked to receive it
 * @param message message
 * @param sender  from sender
 */
void SCDMsgCenter::processMessage(QString msg, QString sender)
{
   Client client;

   for (int n=0; n<clients.size();n++)
   {
      client = clients.at(n);

      if (client.Sender==sender and client.mode==1)
      {
         sendMessageToClient(msg, client.socketDescriptor);
      }
   }
}

/**
 * @brief SCDMsgCenter::removeClient_slot
 * @param socket
 */
void SCDMsgCenter::unregisterClient(int socketDescriptor)
{
   Client client = getClient(socketDescriptor);

   if (client.index > -1)
   {
      clients.removeAt(client.index);
   }
}

/**
 * @brief SCDMsgCenter::removeSender_slot
 * @param sender
 */
void SCDMsgCenter::unregisterMessageSender(QString sender)
{
   senders.removeOne(sender);
}

/**
 * @brief SCDMsgCenter::addClient_slot
 * @param socket
 */
void SCDMsgCenter::registerClient(int socketDescriptor)
{
   Client client = getClient(socketDescriptor);

   if (client.index==-1) // client not exixst
   {
      client.socketDescriptor = socketDescriptor;

      client.name   = "Host-" + QString::number(socketDescriptor);
      client.user   = "Anonymous";
      client.admin  = 0;
      client.mode   = 0; // console

      clients.append(client);

      QString msg = "\n\nMessage Center 1.0\n\n" + getHelpString() + getPrompt(socketDescriptor);

      sendMessageToClient(msg, socketDescriptor);
   }
}

/**
 * @brief SCDMsgCenter::addSender_slot insert the sendet into registered sender list. if already inserted do nothing.
 * @param sender
 */
void SCDMsgCenter::registerMessageSender(QString sender)
{
   if (!senders.contains(sender))
   {
      senders.append(sender);
   }
}

/**
 * @brief SCDMsgCenter::processCommand Process client command
 * @param cmd command emit by client
 * @param socketDescriptor client id
 */
void SCDMsgCenter::processCommand(QString cmd, int clientSocketDescriptor)
{
   QStringList list;

   Client client = getClient(clientSocketDescriptor);

   list = cmd.split(" ",QString::SkipEmptyParts,Qt::CaseInsensitive);

   cmd = list[0];

   cmd = cmd.toLower();

   if (cmd=="\r\n" || cmd=="\n") // update client mode => console mode
   {
      client.mode=0;
      clients.replace(client.index,client);

      sendMessageToClient("\n" + getHelpString() + getPrompt(clientSocketDescriptor),clientSocketDescriptor);
   }
   else
   if (cmd.trimmed()=="spy") // spy the message sender 'sender'
   {
      if (list.size()>1)
      {
         QString sender = list[1].trimmed();

         if (senders.contains(sender))
         {
            client.Sender = sender;
            client.mode   = 1;

            clients.replace(client.index,client);
         }
         else
         {
            sendMessageToClient("\nSender not found: " + sender + getPrompt(clientSocketDescriptor) ,clientSocketDescriptor);
         }
      }
   }
   else
   if (cmd.trimmed()=="exit") // close a message server client socket
   {
      sendMessageToClient(cmd,clientSocketDescriptor);
   }
   else
   if (cmd.trimmed()=="list")  // get the command menu prompt
   {
      QString msg = "\n";

      for (int n=0; n<senders.size();n++)
      {
         msg += "   - ";
         msg += senders.at(n);
         msg += "\n";
      }

      msg += getPrompt(clientSocketDescriptor);

      sendMessageToClient(msg,clientSocketDescriptor);
   }
   else
   if (cmd.trimmed()=="ping") // test the client socket connection
   {
      sendMessageToClient("pong",clientSocketDescriptor);
   }
   else
   if (cmd.trimmed()=="help")
   {
      sendMessageToClient("\n" + getHelpString() + getPrompt(clientSocketDescriptor),clientSocketDescriptor);
   }
   else
   if(cmd.trimmed().at(0)=='@') // send a command to sender 'sender' and enter in 'spy' mode
   {
      QString sender = cmd.trimmed().remove(0,1);

      list.removeFirst();

      cmd = list.join(" ").trimmed();

      if (senders.contains(sender))
      {
         client.Sender = sender;
         client.mode   = 1;

         clients.replace(client.index,client);

         emit commandToSender_signal(cmd,sender);
      }
      else
      {
         sendMessageToClient("\nSender not found: " + sender + getPrompt(clientSocketDescriptor), clientSocketDescriptor);
      }
   }
   else
   {
      if (client.index>-1)
      {
         sendMessageToClient(cmd,clientSocketDescriptor);
      }
   }
}
