from fastapi import APIRouter
from fastapi.responses import JSONResponse
from motor.motor_asyncio import AsyncIOMotorClient
from pydantic import BaseModel
from datetime import datetime, timedelta
import os

router = APIRouter()
client = AsyncIOMotorClient(os.getenv("MONGODB_URI"))
db = client["locker_system"]


class UnlockRequest(BaseModel):
    code: str


@router.post("/api/unlock")
async def unlock(data: UnlockRequest):
    now_vn = datetime.utcnow() + timedelta(hours=7)
    locker = await db.locker.find_one({"code": data.code})
    if not locker:
        return JSONResponse(content={"error": "Invalid code"}, status_code=404)

    # ✅ Chỉ tăng times nếu tủ đang bị khóa
    if locker.get("isLocked", True):
        await db.locker.update_one(
            {"_id": locker["_id"]}, {"$set": {"isLocked": False}, "$inc": {"times": 1}}
        )
    else:
        await db.locker.update_one(
            {"_id": locker["_id"]}, {"$set": {"isLocked": False}}
        )

    await db.locker_history.insert_one(
        {
            "lockerId": str(locker["_id"]),
            "lockerNumber": locker["number"],
            "action": "unlock",
            "timestamp": now_vn,
            "code": data.code,
        }
    )

    updated_locker = await db.locker.find_one({"_id": locker["_id"]})
    updated_locker["_id"] = str(updated_locker["_id"])

    return {"message": "Locker unlocked for 5 seconds", "locker": updated_locker}
