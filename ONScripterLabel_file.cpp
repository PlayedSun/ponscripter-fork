/* -*- C++ -*-
 *
 *  ONScripterLabel_file.cpp - FILE I/O of PONScripter
 *
 *  Copyright (c) 2001-2006 Ogapee (original ONScripter, of which this is a fork).
 *
 *  ogapee@aqua.dti2.ne.jp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ONScripterLabel.h"

#if defined(LINUX) || defined(MACOSX)
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#elif defined(WIN32)
#include <windows.h>
#elif defined(MACOS9)
#include <DateTimeUtils.h>
#include <Files.h>
extern "C" void c2pstrcpy(Str255 dst, const char *src);	//#include <TextUtils.h>
#elif defined(PSP)
#include <pspiofilemgr.h>
#endif

#define SAVEFILE_MAGIC_NUMBER "ONS"
#define SAVEFILE_VERSION_MAJOR 2
#define SAVEFILE_VERSION_MINOR 2

#define READ_LENGTH 4096

void ONScripterLabel::searchSaveFile( SaveFileInfo &save_file_info, int no )
{
    char file_name[256];

    script_h.getStringFromInteger( save_file_info.sjis_no, no, (num_save_file >= 10)?2:1 );
#if defined(LINUX) || defined(MACOSX)
    sprintf( file_name, "%ssave%d.dat", script_h.save_path, no );
    struct stat buf;
    struct tm *tm;
    if ( stat( file_name, &buf ) != 0 ){
        save_file_info.valid = false;
        return;
    }
    tm = localtime( &buf.st_mtime );

    save_file_info.month  = tm->tm_mon + 1;
    save_file_info.day    = tm->tm_mday;
    save_file_info.hour   = tm->tm_hour;
    save_file_info.minute = tm->tm_min;
#elif defined(WIN32)
    sprintf( file_name, "%ssave%d.dat", script_h.save_path, no );
    HANDLE  handle;
    FILETIME    tm, ltm;
    SYSTEMTIME  stm;

    handle = CreateFile( file_name, GENERIC_READ, 0, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if ( handle == INVALID_HANDLE_VALUE ){
        save_file_info.valid = false;
        return;
    }

    GetFileTime( handle, NULL, NULL, &tm );
    FileTimeToLocalFileTime( &tm, &ltm );
    FileTimeToSystemTime( &ltm, &stm );
    CloseHandle( handle );

    save_file_info.month  = stm.wMonth;
    save_file_info.day    = stm.wDay;
    save_file_info.hour   = stm.wHour;
    save_file_info.minute = stm.wMinute;
#elif defined(MACOS9)
	sprintf( file_name, "%ssave%d.dat", script_h.save_path, no );
	CInfoPBRec  pb;
	Str255      p_file_name;
	FSSpec      file_spec;
	DateTimeRec tm;
	c2pstrcpy( p_file_name, file_name );
	if ( FSMakeFSSpec(0, 0, p_file_name, &file_spec) != noErr ){
		save_file_info.valid = false;
		return;
	}
	pb.hFileInfo.ioNamePtr = file_spec.name;
	pb.hFileInfo.ioVRefNum = file_spec.vRefNum;
	pb.hFileInfo.ioFDirIndex = 0;
	pb.hFileInfo.ioDirID = file_spec.parID;
	if (PBGetCatInfoSync(&pb) != noErr) {
		save_file_info.valid = false;
		return;
	}
	SecondsToDate( pb.hFileInfo.ioFlMdDat, &tm );
	save_file_info.month  = tm.month;
	save_file_info.day    = tm.day;
	save_file_info.hour   = tm.hour;
	save_file_info.minute = tm.minute;
#elif defined(PSP)
    sprintf( file_name, "%ssave%d.dat", script_h.save_path, no );
    SceIoStat buf;
    if ( sceIoGetstat(file_name, &buf)<0 ){
        save_file_info.valid = false;
        return;
    }

    save_file_info.month  = buf.st_mtime.month;
    save_file_info.day    = buf.st_mtime.day;
    save_file_info.hour   = buf.st_mtime.hour;
    save_file_info.minute = buf.st_mtime.minute;
#else
    sprintf( file_name, "save%d.dat", no );
    FILE *fp;
    if ( (fp = fopen( file_name, "rb" )) == NULL ){
        save_file_info.valid = false;
        return;
    }
    fclose( fp );

    save_file_info.month  = 1;
    save_file_info.day    = 1;
    save_file_info.hour   = 0;
    save_file_info.minute = 0;
#endif
    save_file_info.valid = true;
    script_h.getStringFromInteger( save_file_info.sjis_month,  save_file_info.month,  2 );
    script_h.getStringFromInteger( save_file_info.sjis_day,    save_file_info.day,    2 );
    script_h.getStringFromInteger( save_file_info.sjis_hour,   save_file_info.hour,   2 );
    script_h.getStringFromInteger( save_file_info.sjis_minute, save_file_info.minute, 2, true );
}

int ONScripterLabel::loadSaveFile( int no )
{
    char filename[16];
    sprintf( filename, "save%d.dat", no );
    if (loadFileIOBuf( filename )){
        fprintf( stderr, "can't open save file %s\n", filename );
        return -1;
    }

    char *str = NULL;
    int  i, j, k, address;
    int  file_version;

    /* ---------------------------------------- */
    /* Load magic number */
    for ( i=0 ; i<(int)strlen( SAVEFILE_MAGIC_NUMBER ) ; i++ )
        if ( readChar() != SAVEFILE_MAGIC_NUMBER[i] ) break;
    if ( i != (int)strlen( SAVEFILE_MAGIC_NUMBER ) ){ // if not ONS save file
        file_io_buf_ptr = 0;
        // check for ONS version 0
        bool ons_ver0_flag = false;
        if ( readInt() != 1 ) ons_ver0_flag = true;
        readInt();
        readInt();
        if ( readInt() != 0 ) ons_ver0_flag = true;
        readInt();
        if ( readInt() != 0xff ) ons_ver0_flag = true;
        if ( readInt() != 0xff ) ons_ver0_flag = true;
        if ( readInt() != 0xff ) ons_ver0_flag = true;

        file_io_buf_ptr = 0;
        if ( !ons_ver0_flag ){
            return loadSaveFile2( SAVEFILE_VERSION_MAJOR*100 + SAVEFILE_VERSION_MINOR );
        }
        file_version = 0;
    }
    else{
        file_version = readChar() * 100;
        file_version += readChar();
    }
    if ( file_version > SAVEFILE_VERSION_MAJOR*100 + SAVEFILE_VERSION_MINOR ){
        fprintf( stderr, "Save file is newer than %d.%d, please use the latest PONScripter.\n", SAVEFILE_VERSION_MAJOR, SAVEFILE_VERSION_MINOR );
        return -1;
    }

    if ( file_version >= 200 )
        return loadSaveFile2( file_version );

    deleteNestInfo();

    /* ---------------------------------------- */
    /* Load text history */
// FIXME: this will (probably) NOT work with UTF-8 and proportional text...
    if ( file_version >= 107 ) readInt();
    int text_history_num = readInt();
    for ( i=0 ; i<text_history_num ; i++ ){
        int num_xy[2];
        num_xy[0] = readInt();
        num_xy[1] = readInt();
        //current_text_buffer->num = (num_xy[0]*2+1)*num_xy[1];
        int xy[2];
        xy[0] = readInt();
        xy[1] = readInt();
        current_text_buffer->clear();

        char ch1, ch2;
        for ( j=0, k=0 ; j<num_xy[0] * num_xy[1] ; j++ ){
            ch1 = readChar();
            ch2 = readChar();
            if ( ch1 == ((char*)"�@")[0] &&
                 ch2 == ((char*)"�@")[1] ){
                k += 2;
            }
            else{
                if ( ch1 ){
                    current_text_buffer->addBuffer( ch1 );
                    k++;
                }
                if ( ch1 & 0x80 || ch2 ){
                    current_text_buffer->addBuffer( ch2 );
                    k++;
                }
            }
            if ( k >= num_xy[0] * 2 ){
                current_text_buffer->addBuffer( 0x0a );
                k = 0;
            }
        }
        current_text_buffer = current_text_buffer->next;
        if ( i==0 ){
            for ( j=0 ; j<max_text_buffer - text_history_num ; j++ )
                current_text_buffer = current_text_buffer->next;
            start_text_buffer = current_text_buffer;
        }
    }

    /* ---------------------------------------- */
    /* Load sentence font */
    j = readInt();
    //sentence_font.is_valid = (j==1)?true:false;
    sentence_font.font_size = readInt();
    if ( file_version >= 100 ){
        sentence_font.font_size_mod = readInt();
    }
    else{
        sentence_font.font_size_mod = 0;
    }
    sentence_font.top_x = readInt();
    sentence_font.top_y = readInt();
    sentence_font.area_x = readInt();
    sentence_font.area_y = readInt();
    const int px = readInt();
    sentence_font.SetXY(px, readInt());
    sentence_font.pitch_x = readInt();
    sentence_font.pitch_y = readInt();
    sentence_font.wait_time = readInt();
    sentence_font.is_bold = (readInt()==1)?true:false;
    sentence_font.is_shadow = (readInt()==1)?true:false;
    sentence_font.is_transparent = (readInt()==1)?true:false;

// FIXME: not sure what this does, but it'll be broken.
// Actually, looking at it, it doesn't appear to be essential...?
//    const char *buffer = current_text_buffer->contents.c_str();
//    int count = current_text_buffer->contents.size();
//    for (j=0, k=0, i=0 ; i<count ; i++){
//        if (j == sentence_font.pos_y &&
//            (k > sentence_font.pos_x ||
//             buffer[i] == 0x0a)) break;
//
//        if (buffer[i] == 0x0a){
//            j+=2;
//            k=0;
//        }
//        else
//            k++;
//    }
//    current_text_buffer->buffer2_count = i;

    /* Dummy, must be removed later !! */
    for ( i=0 ; i<8 ; i++ ){
        j = readInt();
        //sentence_font.window_color[i] = j;
    }
    /* Should be char, not integer !! */
    for ( i=0 ; i<3 ; i++ )
        sentence_font.window_color[i] = readInt();
    readStr( &sentence_font_info.image_name );

    sentence_font_info.pos.x = readInt() * screen_ratio1 / screen_ratio2;
    sentence_font_info.pos.y = readInt() * screen_ratio1 / screen_ratio2;
    sentence_font_info.pos.w = readInt() * screen_ratio1 / screen_ratio2;
    sentence_font_info.pos.h = readInt() * screen_ratio1 / screen_ratio2;

    if ( !sentence_font.is_transparent ){
        parseTaggedString( &sentence_font_info );
        setupAnimationInfo( &sentence_font_info );
    }

    clickstr_state = readInt();
    new_line_skip_flag = (readInt()==1)?true:false;
    if ( file_version >= 103 ){
        erase_text_window_mode = readInt();
    }

    /* ---------------------------------------- */
    /* Load link label info */

    int offset = 0;
    while( 1 ){
        readStr( &str );
        current_label_info = script_h.lookupLabel( str );

        current_line = readInt() + 2;
        char *buf = current_label_info.label_header;
        while( buf < current_label_info.start_address ){
            if ( *buf == 0x0a ) current_line--;
            buf++;
        }

        offset = readInt();

        script_h.setCurrent( current_label_info.label_header );
        script_h.skipLine( current_line );

        if ( file_version <= 104 )
        {
            if ( file_version >= 102 )
                readInt();

            address = readInt();
        }
        else{
            offset += readInt();
        }

        if ( readChar() == 0 ) break;

        last_nest_info->next = new NestInfo();
        last_nest_info->next->previous = last_nest_info;
        last_nest_info = last_nest_info->next;
        last_nest_info->next_script = script_h.getCurrent() + offset;
    }
    script_h.setCurrent( script_h.getCurrent() + offset );

    int tmp_event_mode = readChar();

    /* ---------------------------------------- */
    /* Load variables */
    readVariables( 0, 200 );

    /* ---------------------------------------- */
    /* Load monocro flag */
    monocro_flag = (readChar()==1)?true:false;
    if ( file_version >= 101 ){
        monocro_flag = (readChar()==1)?true:false;
    }
    for ( i=0 ; i<3 ; i++ ) monocro_color[i] = readChar();

    if ( file_version >= 101 ){
        for ( i=0 ; i<3 ; i++ ) monocro_color[i] = readChar();
        readChar(); // obsolete, need_refresh_flag
    }
    for ( i=0 ; i<256 ; i++ ){
        monocro_color_lut[i][0] = (monocro_color[0] * i) >> 8;
        monocro_color_lut[i][1] = (monocro_color[1] * i) >> 8;
        monocro_color_lut[i][2] = (monocro_color[2] * i) >> 8;
    }

    /* Load nega flag */
    if ( file_version >= 104 ){
        nega_mode = (unsigned char)readChar();
    }

    /* ---------------------------------------- */
    /* Load current images */
    bg_info.remove();
    bg_info.color[0] = (unsigned char)readChar();
    bg_info.color[1] = (unsigned char)readChar();
    bg_info.color[2] = (unsigned char)readChar();
    bg_info.num_of_cells = 1;
    readStr( &bg_info.file_name );
    setupAnimationInfo( &bg_info );
    bg_effect_image = (EFFECT_IMAGE)readChar();

    if (bg_effect_image == COLOR_EFFECT_IMAGE){
        bg_info.allocImage( screen_width, screen_height );
        bg_info.fill( bg_info.color[0], bg_info.color[1], bg_info.color[2], 0xff );
        bg_info.pos.x = 0;
        bg_info.pos.y = 0;
    }
    bg_info.trans_mode = AnimationInfo::TRANS_COPY;

    for ( i=0 ; i<3 ; i++ )
        tachi_info[i].remove();

    for ( i=0 ; i<MAX_SPRITE_NUM ; i++ )
        sprite_info[i].remove();

    /* ---------------------------------------- */
    /* Load Tachi image and Sprite */
    for ( i=0 ; i<3 ; i++ ){
        readStr( &tachi_info[i].image_name );
        if ( tachi_info[i].image_name ){
            parseTaggedString( &tachi_info[i] );
            setupAnimationInfo( &tachi_info[ i ] );
            tachi_info[ i ].pos.x = screen_width * (i+1) / 4 - tachi_info[ i ].pos.w / 2;
            tachi_info[ i ].pos.y = underline_value - tachi_info[ i ].image_surface->h + 1;
        }
    }

    /* ---------------------------------------- */
    /* Load current sprites */
    for ( i=0 ; i<256 ; i++ ){
        sprite_info[i].visible = (readInt()==1)?true:false;
        sprite_info[i].pos.x = readInt() * screen_ratio1 / screen_ratio2;
        sprite_info[i].pos.y = readInt() * screen_ratio1 / screen_ratio2;
        sprite_info[i].trans = readInt();
        readStr( &sprite_info[i].image_name );
        if ( sprite_info[i].image_name ){
            parseTaggedString( &sprite_info[i] );
            setupAnimationInfo( &sprite_info[i] );
        }
    }

    /* ---------------------------------------- */
    /* Load current playing CD track */
    stopCommand();
    loopbgmstopCommand();

    current_cd_track = (Sint8)readChar();
    bool play_once_flag = (readChar()==1)?true:false;
    if ( current_cd_track == -2 ){
        readStr( &midi_file_name );
        midi_play_loop_flag = !play_once_flag;
        setStr( &music_file_name, NULL );
        music_play_loop_flag = false;
    }
    else{
        readStr( &music_file_name );
        if ( music_file_name ){
            music_play_loop_flag = !play_once_flag;
            cd_play_loop_flag = false;
        }
        else{
            music_play_loop_flag = false;
            cd_play_loop_flag = !play_once_flag;
        }
        setStr( &midi_file_name, NULL );
        midi_play_loop_flag = false;
    }

    if ( current_cd_track >= 0 ){
        playCDAudio();
    }
    else if ( midi_file_name && midi_play_loop_flag ){
        playSound(midi_file_name, SOUND_MIDI, midi_play_loop_flag);
    }
    else if ( music_file_name && music_play_loop_flag ){
        playSound(music_file_name,
                  SOUND_WAVE | SOUND_OGG_STREAMING | SOUND_MP3,
                  music_play_loop_flag, MIX_BGM_CHANNEL);
    }

    /* ---------------------------------------- */
    /* Load rmode flag */
    rmode_flag = (readChar()==1)?true:false;

    /* ---------------------------------------- */
    /* Load text on flag */
    text_on_flag = (readChar()==1)?true:false;


    restoreTextBuffer();
    num_chars_in_sentence = 0;
    cached_text_buffer = current_text_buffer;

    display_mode = next_display_mode = TEXT_DISPLAY_MODE;
    current_refresh_mode = REFRESH_NORMAL_MODE;

    event_mode = tmp_event_mode;
    if ( event_mode & WAIT_BUTTON_MODE ) event_mode = WAIT_SLEEP_MODE; // Re-execute the selectCommand, etc.

    if ( event_mode & WAIT_SLEEP_MODE )
        event_mode &= ~WAIT_SLEEP_MODE;
    else
        event_mode |= WAIT_TIMER_MODE;
    if (event_mode & WAIT_INPUT_MODE) event_mode |= WAIT_TEXT_MODE;

    draw_cursor_flag = (clickstr_state == CLICK_NONE)?false:true;

    return 0;
}

void ONScripterLabel::saveMagicNumber( bool output_flag )
{
    for ( unsigned int i=0 ; i<strlen( SAVEFILE_MAGIC_NUMBER ) ; i++ )
        writeChar( SAVEFILE_MAGIC_NUMBER[i], output_flag );
    writeChar( SAVEFILE_VERSION_MAJOR, output_flag );
    writeChar( SAVEFILE_VERSION_MINOR, output_flag );
}

int ONScripterLabel::saveSaveFile( int no )
{
    // make save data structure on memory
    if (no < 0 || saveon_flag && internal_saveon_flag){
        file_io_buf_ptr = 0;
        saveMagicNumber( false );
        saveSaveFile2( false );
        allocFileIOBuf();
        saveMagicNumber( true );
        saveSaveFile2( true );
        save_data_len = file_io_buf_ptr;
        memcpy(save_data_buf, file_io_buf, save_data_len);
    }

    if ( no >= 0 ){
        saveAll();

        char filename[16];
        sprintf( filename, "save%d.dat", no );

        memcpy(file_io_buf, save_data_buf, save_data_len);
        file_io_buf_ptr = save_data_len;
        if (saveFileIOBuf( filename )){
            fprintf( stderr, "can't open save file %s for writing\n", filename );
            return -1;
        }

        size_t magic_len = strlen(SAVEFILE_MAGIC_NUMBER)+2;
        sprintf( filename, RELATIVEPATH "sav%csave%d.dat", DELIMITER, no );
        saveFileIOBuf( filename, magic_len );
    }

    return 0;
}
