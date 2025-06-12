from db import db
from fastapi import APIRouter, BackgroundTasks
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from datetime import datetime, timedelta
import asyncio


router = APIRouter()


class UnlockRequest(BaseModel):
    code: str


async def set_locker_locked(locker_id):
    """Khóa tủ sau 5 giây."""
    await asyncio.sleep(10)
    await db.locker.update_one({"_id": locker_id}, {"$set": {"isLocked": True}})


@router.post("/api/unlock")
async def unlock(data: UnlockRequest, background_tasks=BackgroundTasks):
    now_vn = datetime.utcnow() + timedelta(hours=7)
    locker = await db.locker.find_one({"code": data.code})
    if not locker:
        return JSONResponse(content={"error": "Invalid code"}, status_code=404)

    # ✅ Chỉ tăng times nếu tủ đang bị khóa
    update_data = {"isLocked": False}
    if locker.get("isLocked", True):
        update_data["times"] = locker.get("times", 0) + 1
    await db.locker.update_one({"_id": locker["_id"]}, {"$set": update_data})

    await db.locker_history.insert_one(
        {
            "lockerId": str(locker["_id"]),
            "lockerNumber": locker["number"],
            "action": "unlock",
            "timestamp": now_vn,
            "code": data.code,
        }
    )

    background_tasks.add_task(set_locker_locked, locker["_id"])

    updated_locker = await db.locker.find_one({"_id": locker["_id"]})
    updated_locker["_id"] = str(updated_locker["_id"])

    return {"message": "Locker unlocked for 5 seconds", "locker": updated_locker}
