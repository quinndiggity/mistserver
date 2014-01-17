#include"oggconv.h"
#include <stdlib.h>
#include <mist/bitstream.h>


namespace OGG{
  void converter::readDTSCHeader(DTSC::Meta & meta){
    //pages.clear();
    parsedPages = "";
    Page curOggPage;
    srand (Util::getMS());//randomising with milliseconds from boot
    std::vector<unsigned int> curSegTable;
    //trackInf.clear();
    //Creating ID headers for theora and vorbis
    for ( std::map<int,DTSC::Track>::iterator it = meta.tracks.begin(); it != meta.tracks.end(); it ++) {
      curOggPage.clear();
      curOggPage.setVersion();
      curOggPage.setHeaderType(2);//headertype 2 = Begin of Stream
      curOggPage.setGranulePosition(0);
      trackInf[it->second.trackID].OGGSerial = rand() % 0xFFFFFFFE +1; //initialising on a random not 0 number
      curOggPage.setBitstreamSerialNumber(trackInf[it->second.trackID].OGGSerial);
      trackInf[it->second.trackID].seqNum = 0;
      curOggPage.setPageSequenceNumber(trackInf[it->second.trackID].seqNum++);
      curSegTable.clear();
      curSegTable.push_back(it->second.idHeader.size());
      curOggPage.setSegmentTable(curSegTable);
      curOggPage.setPayload((char*)it->second.idHeader.c_str(), it->second.idHeader.size());
      curOggPage.setCRCChecksum(curOggPage.calcChecksum());
      //pages.push_back(curOggPage);
      parsedPages += std::string(curOggPage.getPage(), curOggPage.getPageSize());
      trackInf[it->second.trackID].codec = it->second.codec;
      if (it->second.codec == "theora"){
        trackInf[it->second.trackID].lastKeyFrame = 1;
        trackInf[it->second.trackID].sinceKeyFrame = 0;
        theora::header tempHead;
        std::string tempString = it->second.idHeader;
        tempHead.read((char*)tempString.c_str(),42);
        trackInf[it->second.trackID].significantValue = tempHead.getKFGShift();
      }else if (it->second.codec == "vorbis"){
        trackInf[it->second.trackID].lastKeyFrame = 0;
        trackInf[it->second.trackID].sinceKeyFrame = 0;
        trackInf[it->second.trackID].prevBlockFlag = -1;
        vorbis::header tempHead;
        std::string tempString = it->second.idHeader;
        tempHead.read((char*)tempString.c_str(),tempString.size());
        trackInf[it->second.trackID].significantValue = tempHead.getAudioSampleRate() / tempHead.getAudioChannels();
        if (tempHead.getBlockSize0() <= tempHead.getBlockSize1()){
          trackInf[it->second.trackID].blockSize[0] = tempHead.getBlockSize0();
          trackInf[it->second.trackID].blockSize[1] = tempHead.getBlockSize1();
        }else{
          trackInf[it->second.trackID].blockSize[0] = tempHead.getBlockSize1();
          trackInf[it->second.trackID].blockSize[1] = tempHead.getBlockSize0();
        }
        char audioChannels = tempHead.getAudioChannels();
        //getting modes
        tempString = it->second.init;
        tempHead.read((char*)tempString.c_str(),tempString.size());
        trackInf[it->second.trackID].vorbisModes = tempHead.readModeDeque(audioChannels);
        trackInf[it->second.trackID].hadFirst = false;
      }
    }
    //Creating remaining headers for theora and vorbis
    //for tracks in header
    //create standard page with comment (empty) en setup header(init)
    for ( std::map<int,DTSC::Track>::iterator it = meta.tracks.begin(); it != meta.tracks.end(); it ++) {
      curOggPage.clear();
      curOggPage.setVersion();
      curOggPage.setHeaderType(0);//headertype 0 = normal
      curOggPage.setGranulePosition(0);
      curOggPage.setBitstreamSerialNumber(trackInf[it->second.trackID].OGGSerial);
      curOggPage.setPageSequenceNumber(trackInf[it->second.trackID].seqNum++);
      curSegTable.clear();
      curSegTable.push_back(it->second.commentHeader.size());
      curSegTable.push_back(it->second.init.size());
      curOggPage.setSegmentTable(curSegTable);
      std::string fullHeader = it->second.commentHeader + it->second.init;
      curOggPage.setPayload((char*)fullHeader.c_str(),fullHeader.size());
      curOggPage.setCRCChecksum(curOggPage.calcChecksum());
      parsedPages += std::string(curOggPage.getPage(), curOggPage.getPageSize());
    }
  }
  
  std::string converter::readDTSCVector(std::vector <JSON::Value> DTSCVec){
    Page retVal;
    int typeFlag = 0;//flag to remember if the page has a continued segment
    std::string pageBuffer = "";
    long long int DTSCID = DTSCVec[0]["trackid"].asInt();
    std::vector<unsigned int> curSegTable;
    std::string dataBuffer;
    long long unsigned int lastGran = 0;

    for (unsigned int i = 0; i < DTSCVec.size(); i++){
      OGG::Page tempPage;
      tempPage.setSegmentTable(curSegTable);
      if (DTSCVec[i]["data"].asString().size() >= (255-tempPage.getPageSegments())*255u){//if segment is too big
        //Put page in Buffer and start next page
        if (!curSegTable.empty()){
          //output page
          retVal.clear();
          retVal.setVersion();
          retVal.setHeaderType(typeFlag);//headertype 0 = normal
          retVal.setGranulePosition(lastGran);
          retVal.setBitstreamSerialNumber(trackInf[DTSCID].OGGSerial);
          retVal.setPageSequenceNumber(trackInf[DTSCID].seqNum);
          retVal.setSegmentTable(curSegTable);
          retVal.setPayload((char*)dataBuffer.c_str(), dataBuffer.size());
          retVal.setCRCChecksum(retVal.calcChecksum());
          trackInf[DTSCID].seqNum++;
          pageBuffer += std::string((char*)retVal.getPage(), retVal.getPageSize());
          
          curSegTable.clear();
          dataBuffer = "";
        }
        std::string remainingData = DTSCVec[i]["data"].asString();
        typeFlag = 0;
        while (remainingData.size() > 255*255){
          //output part of the segment
          //granule -1
          curSegTable.clear();
          curSegTable.push_back(255*255);
          retVal.clear();
          retVal.setVersion();
          retVal.setHeaderType(typeFlag);//normal Page
          retVal.setGranulePosition(-1);
          retVal.setBitstreamSerialNumber(trackInf[DTSCID].OGGSerial);
          retVal.setPageSequenceNumber(trackInf[DTSCID].seqNum);
          retVal.setSegmentTable(curSegTable);
          retVal.setPayload((char*)remainingData.substr(0,255*255).c_str(), 255*255);
          retVal.setCRCChecksum(retVal.calcChecksum());
          trackInf[DTSCID].seqNum++;
          pageBuffer += std::string((char*)retVal.getPage(), retVal.getPageSize());
          remainingData = remainingData.substr(255*255);
          typeFlag = 1;//1 = continued page
        }
        //output last remaining data
        curSegTable.clear();
        curSegTable.push_back(remainingData.size());
        dataBuffer += remainingData;
      }else{//build data for page
        curSegTable.push_back(DTSCVec[i]["data"].asString().size());
        dataBuffer += DTSCVec[i]["data"].asString();
      }
      //lastGran = calcGranule(DTSCID, DTSCVec[i]["keyframe"].asBool());
      //calculating granule position
      if (trackInf[DTSCID].codec == "theora"){
        if (DTSCVec[i]["keyframe"].asBool()){
          trackInf[DTSCID].lastKeyFrame += trackInf[DTSCID].sinceKeyFrame + 1;
          trackInf[DTSCID].sinceKeyFrame = 0;
        }else{
          trackInf[DTSCID].sinceKeyFrame ++;
        }
        lastGran = (trackInf[DTSCID].lastKeyFrame << trackInf[DTSCID].significantValue) + trackInf[DTSCID].sinceKeyFrame;
      } else if (trackInf[DTSCID].codec == "vorbis"){
        //decode DTSCVec[i]["data"].asString() for mode index
        Utils::bitstreamLSBF packet;
        packet.append(DTSCVec[i]["data"].asString());
        //calculate amount of samples associated with that block (from ID header)
        //check mode block in deque for index
        int curPCMSamples = 0;
        if (packet.get(1) == 0){
          int tempModes = vorbis::ilog(trackInf[DTSCID].vorbisModes.size()-1);
          int tempPacket = packet.get(tempModes);
          int curBlockFlag = trackInf[DTSCID].vorbisModes[tempPacket].blockFlag;
          curPCMSamples = (1 << trackInf[DTSCID].blockSize[curBlockFlag]);
          if (trackInf[DTSCID].prevBlockFlag!= -1){
            if (curBlockFlag == trackInf[DTSCID].prevBlockFlag){
              curPCMSamples /= 2;
            }else{
              curPCMSamples -= (1 << trackInf[DTSCID].blockSize[0]) / 4 + (1 << trackInf[DTSCID].blockSize[1]) / 4;
            }
          }
          trackInf[DTSCID].sinceKeyFrame = (1 << trackInf[DTSCID].blockSize[curBlockFlag]);
          trackInf[DTSCID].prevBlockFlag = curBlockFlag;
        }else{
          std::cerr << "Error, Vorbis packet type !=0" << std::endl;
        }
        //add to granule position
        trackInf[DTSCID].lastKeyFrame += curPCMSamples;
        lastGran  = trackInf[DTSCID].lastKeyFrame;
      }
    }
    //last parts of page put out 
    if (!curSegTable.empty()){
      retVal.clear();
      retVal.setVersion();
      retVal.setHeaderType(typeFlag);//headertype 0 = normal
      retVal.setGranulePosition(lastGran);
      retVal.setBitstreamSerialNumber(trackInf[DTSCID].OGGSerial);
      retVal.setPageSequenceNumber(trackInf[DTSCID].seqNum);
      retVal.setSegmentTable(curSegTable);
      retVal.setPayload((char*)dataBuffer.c_str(), dataBuffer.size());
      retVal.setCRCChecksum(retVal.calcChecksum());
      trackInf[DTSCID].seqNum++;
      pageBuffer += std::string((char*)retVal.getPage(), retVal.getPageSize());
    }
    return pageBuffer;
  }
}