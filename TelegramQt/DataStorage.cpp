/*
   Copyright (C) 2018 Alexander Akulich <akulichalexander@gmail.com>

   This file is a part of TelegramQt library.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

 */

#include "DataStorage_p.hpp"

namespace Telegram {

namespace Client {

DataStorage::DataStorage(QObject *parent)
    : QObject(parent)
{
}

DcConfiguration DataStorage::serverConfiguration() const
{
    Q_D(const DataStorage);
    return d->m_serverConfig;
}

void DataStorage::setServerConfiguration(const DcConfiguration &configuration)
{
    Q_D(DataStorage);
    d->m_serverConfig = configuration;
}

} // Client namespace

} // Telegram namespace