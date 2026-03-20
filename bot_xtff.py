import telebot
import json
import os
import requests
import re  # thêm regex để parse key tốt hơn

# ================== THAY Ở ĐÂY ==================
BOT_TOKEN = "8494428727:AAHtD4bUotm_UVEnaWeWtnnBDMPAOGgSX8U"          # Token từ @BotFather
GETKEY_URL = "https://ayta09.com/keys/getkey?admin=Yuri58208"  # Link getkey của mày
OWNER_ID = 8610148719                       # ID Telegram của mày (lấy từ @userinfobot)
# ===============================================

DATA_FILE = "keys.json"
bot = telebot.TeleBot(BOT_TOKEN)

def load_data():
    if os.path.exists(DATA_FILE):
        with open(DATA_FILE, "r", encoding="utf-8") as f:
            return json.load(f)
    return {"available": [], "sold": []}

def save_data(data):
    with open(DATA_FILE, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=4)

data = load_data()

def fetch_new_keys(so_luong=10):
    new_keys = []
    try:
        for _ in range(so_luong):
            r = requests.get(GETKEY_URL, timeout=15)
            if r.status_code == 200:
                text = r.text.strip()
                if not text:
                    continue
                
                # Parse: loại bỏ HTML tag cơ bản, tìm chuỗi alphanumeric dài (key thường 10-50 ký tự)
                clean_text = re.sub(r'<[^>]+>', '', text)  # strip HTML
                potential_keys = re.findall(r'[A-Za-z0-9\-_]{10,}', clean_text)  # tìm key-like strings
                
                if potential_keys:
                    new_keys.extend(potential_keys)
                
                # Nếu có prefix như "Key:", "Activation:", lấy phần sau
                if "Key:" in clean_text or "key:" in clean_text:
                    parts = re.split(r'(?i)key[:\s=]+', clean_text)
                    if len(parts) > 1:
                        key_part = parts[1].strip().split()[0]
                        if len(key_part) > 10:
                            new_keys.append(key_part)
                
                if len(new_keys) >= so_luong:
                    break
    except Exception as e:
        print(f"Lỗi fetch key: {e}")
    return list(set(new_keys))[:so_luong]  # loại trùng + giới hạn

def clean_sold_if_needed():
    global data
    if len(data["sold"]) > 7:
        old = len(data["sold"])
        data["sold"] = data["sold"][-3:]  # giữ 3 mới nhất
        save_data(data)
        print(f"Dọn sold: {old} → {len(data['sold'])}")

# ================== COMMAND ==================
@bot.message_handler(commands=['start'])
def start(message):
    bot.reply_to(message, "✅ Bot key XTFF Free Fire sẵn sàng!\n/gợi ý: /getkey để nhận key.\nGiá 20k/key nhé bro!")

@bot.message_handler(commands=['getkey'])
def getkey(message):
    global data
    data = load_data()

    if len(data["available"]) < 3:
        bot.reply_to(message, "⚠️ Kho sắp hết, đang refill key mới...")
        data["available"] = []
        moi = fetch_new_keys(10)
        data["available"].extend(moi)
        save_data(data)
        
        if not moi:
            bot.reply_to(message, "❌ Lỗi lấy key từ server, thử lại sau!")
            return
        bot.reply_to(message, f"✅ Thêm {len(moi)} key mới vào kho!")

    clean_sold_if_needed()

    if not data["available"]:
        bot.reply_to(message, "❌ Hết key rồi bro, chờ refill!")
        return

    key = data["available"].pop(0)
    data["sold"].append(key)
    save_data(data)

    bot.reply_to(message, f"🔑 **Key XTFF của bạn:**\n`{key}`\nGhi chú: Giá 20k/key nhé bro, hỗ trợ thêm nếu cần!\nDùng ngay, giữ kỹ!")

    try:
        bot.send_message(OWNER_ID, f"🛒 Bán key:\n`{key}`\nUser: {message.from_user.id} @{message.from_user.username or 'no username'}")
    except:
        pass

@bot.message_handler(commands=['status'])
def status(message):
    if message.from_user.id != OWNER_ID:
        return
    data = load_data()
    clean_sold_if_needed()
    bot.reply_to(message, f"📊 Kho XTFF:\nAvailable: {len(data['available'])}\nSold (giữ ~7): {len(data['sold'])}")

@bot.message_handler(commands=['sold'])
def show_sold(message):
    if message.from_user.id != OWNER_ID:
        return
    data = load_data()
    clean_sold_if_needed()
    if not data["sold"]:
        bot.reply_to(message, "Chưa bán key nào.")
        return
    text = "📋 Key đã bán (giữ 3 mới nhất):\n" + "\n".join(data["sold"])
    if len(data["sold"]) == 3:
        text += "\n\n(Đã tự dọn, chỉ giữ 3 mới nhất)"
    bot.reply_to(message, text)

@bot.message_handler(commands=['fetch'])
def fetch_manual(message):
    if message.from_user.id != OWNER_ID:
        return
    global data
    data = load_data()
    data["available"] = []
    moi = fetch_new_keys(10)
    data["available"] = moi
    save_data(data)
    bot.reply_to(message, f"✅ Xóa available cũ + thêm {len(moi)} key mới!")

print("Bot XTFF đang chạy...")
bot.infinity_polling()
