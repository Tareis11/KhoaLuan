from db import db
from fastapi import APIRouter
from pydantic import BaseModel
from motor.motor_asyncio import AsyncIOMotorClient
from datetime import datetime, timedelta  # ✅ dùng timedelta để cộng giờ
import os

router = APIRouter()


# Data model cho ESP gửi nhiệt độ
class TemperatureData(BaseModel):
    temperature: float


# ESP gửi nhiệt độ lên
@router.post("/api/temperature")
async def log_system_temperature(data: TemperatureData):
    now_vn = datetime.utcnow() + timedelta(hours=7)  # ✅ giờ Việt Nam (không tzinfo)

    await db.system_temperature.update_one(
        {"_id": "current"},
        {"$set": {"temperature": data.temperature, "lastSeen": now_vn}},
        upsert=True,
    )

    await db.temperature_log.insert_one(
        {"temperature": data.temperature, "timestamp": now_vn}
    )

    return {"status": "ok", "temperature": data.temperature}


# Lấy lịch sử nhiệt độ
@router.get("/api/temperature/history")
async def get_system_temperature_logs():
    logs = await db.temperature_log.find({}).sort("timestamp", -1).to_list(24)
    for log in logs:
        log["_id"] = str(log["_id"])
        log["timestamp"] = log["timestamp"].isoformat()
    return {"temperatureLogs": logs}


# Kiểm tra trạng thái kết nối ESP
@router.get("/api/temperature/status")
async def get_esp_status():
    doc = await db.system_temperature.find_one({"_id": "current"})
    if not doc:
        return {"connected": False, "lastSeen": None}

    last_seen = doc.get("lastSeen")
    if not last_seen:
        return {"connected": False, "lastSeen": None}

    # Vẫn dùng UTC để kiểm tra tính chính xác
    delta = datetime.utcnow() - (last_seen - timedelta(hours=7))
    connected = delta.total_seconds() < 15

    return {
        "connected": connected,
        "lastSeen": last_seen.isoformat(),
        "temperature": doc.get("temperature"),
    }
