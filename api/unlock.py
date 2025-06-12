from db import db
from fastapi import APIRouter, BackgroundTasks
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from datetime import datetime, timedelta
<<<<<<< HEAD
from auto_lock import auto_lock_locker
=======
import asyncio
>>>>>>> c61fb19cd4cc3936e24183dffd75a75212a2a91b

router = APIRouter()


class UnlockRequest(BaseModel):
    code: str


<<<<<<< HEAD
=======
async def set_locker_locked(locker_id, delay=10):
    """Tự động khóa tủ sau delay giây."""
    await asyncio.sleep(delay)
    await db.locker.update_one({"_id": locker_id}, {"$set": {"isLocked": True}})


>>>>>>> c61fb19cd4cc3936e24183dffd75a75212a2a91b
@router.post("/api/unlock")
async def unlock(data: UnlockRequest, background_tasks: BackgroundTasks):
    now_vn = datetime.utcnow() + timedelta(hours=7)
    locker = await db.locker.find_one({"code": data.code})
    if not locker:
        return JSONResponse(content={"error": "Invalid code"}, status_code=404)

    # Chỉ tăng times nếu tủ đang bị khóa
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

    # Tự động khóa tủ sau 10 giây
<<<<<<< HEAD
    background_tasks.add_task(auto_lock_locker, locker["_id"], 10)
=======
    background_tasks.add_task(set_locker_locked, locker["_id"], 10)
>>>>>>> c61fb19cd4cc3936e24183dffd75a75212a2a91b

    updated_locker = await db.locker.find_one({"_id": locker["_id"]})
    updated_locker["_id"] = str(updated_locker["_id"])

    return {"message": "Locker unlocked for 10 seconds", "locker": updated_locker}
