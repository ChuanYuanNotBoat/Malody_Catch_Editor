#include "MetaData.h"

MetaData::MetaData()
    : title("Untitled"), titleOrg(""), artist("Unknown"), artistOrg(""),
      difficulty("Normal"), chartAuthor(""), audioFile(""), backgroundFile(""),
      previewTime(0), firstBpm(120.0), offset(0), speed(1) {}

bool MetaData::isValid() const
{
    return !title.isEmpty() && !artist.isEmpty() && !audioFile.isEmpty();
}