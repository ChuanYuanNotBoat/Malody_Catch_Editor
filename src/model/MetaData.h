#pragma once

#include <QString>

struct MetaData
{
    QString title;
    QString titleOrg;
    QString artist;
    QString artistOrg;
    QString difficulty;
    QString chartAuthor;
    QString audioFile;
    QString backgroundFile;
    int previewTime;
    double firstBpm;
    int offset;
    int speed;

    MetaData();
    bool isValid() const;
};