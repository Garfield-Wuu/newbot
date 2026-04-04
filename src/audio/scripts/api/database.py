import sqlite3
from datetime import datetime
import os
class DataBase:
    def __init__(self):
        db_file_name = os.path.join(os.path.expanduser('~'),".ros/tts.db")
        self.conn = sqlite3.connect(db_file_name) #注意：如果用ROS启动，没写路径会保存在~/.ros/目录下
        self.cursor = self.conn.cursor()
        self.cursor.execute('''CREATE TABLE IF NOT EXISTS tts
                            (id INTEGER PRIMARY KEY AUTOINCREMENT,
                             text TEXT NOT NULL,
                             audio BLOB NOT NULL,
                             created_at TIMESTAMP NOT NULL)''')
        self.conn.commit()

    def get_count(self):
        self.cursor.execute('''SELECT COUNT(*) FROM tts''')
        count = self.cursor.fetchone()[0]
        return count

    def get_audio(self, text):
        self.cursor.execute('''SELECT audio FROM tts WHERE text=?''', (text,))
        result = self.cursor.fetchone()
        if result:
            return result[0]
        else:
            return None

    def save_audio(self, text, audio_data):
        self.cursor.execute('''INSERT INTO tts (text, audio, created_at) VALUES (?, ?, ?)''',
                            (text, audio_data, datetime.now()))
        self.conn.commit()
        self.cleanup_db()

    def cleanup_db(self):
        count = self.get_count()
        max_cnt = 1000 #最大保留1000条离线数据
        if count > max_cnt:
            self.cursor.execute('''DELETE FROM tts WHERE id IN 
                                   (SELECT id FROM tts ORDER BY created_at ASC LIMIT ?)''', (count - max_cnt,))
            self.conn.commit()

    def __del__(self):
        self.conn.close()
