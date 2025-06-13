from db import db
from fastapi import APIRouter, BackgroundTasks
from fastapi.responses import JSONResponse
from pydantic import BaseModel
from datetime import datetime, timedelta
from auto_lock import auto_lock_locker

router = APIRouter()


class UnlockRequest(BaseModel):
    code: str


@router.post("/api/unlock")
async def unlock(data: UnlockRequest, background_tasks: BackgroundTasks):
    now_vn = datetime.utcnow() + timedelta(hours=7)
    locker = await db.locker.find_one({"code": data.code})
    if not locker:
        return JSONResponse(content={"error": "Invalid code"}, status_code=404)

    current_times = locker.get("times", 0)
    is_locked = locker.get("isLocked", True)
    should_reset = current_times >= 1

    if should_reset:
        new_times = 0
        new_code = ""
        message = "Locker reset after 2 unlocks"
        action = "unlock"
    else:
        new_times = current_times + 1 if is_locked else current_times
        new_code = locker.get("code", "")
        message = "Locker unlocked for 10 seconds"
        action = "unlock"

    await db.locker.update_one(
        {"_id": locker["_id"]},
        {"$set": {"isLocked": False, "times": new_times, "code": new_code}},
    )

    await db.locker_history.insert_one(
        {
            "lockerId": str(locker["_id"]),
            "lockerNumber": locker["number"],
            "action": action,
            "code": locker["code"],
            "timestamp": now_vn,
        }
    )

    # Tự động khóa sau 10 giây
    background_tasks.add_task(auto_lock_locker, locker["_id"])

    return {
        "message": message,
        "locker": {
            "_id": str(locker["_id"]),
            "number": locker["number"],
            "isLocked": False,
            "times": new_times,
            "code": new_code,
        },
    }
