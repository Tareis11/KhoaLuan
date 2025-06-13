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

    # Nếu số lần mở >= 2 thì reset locker
    if locker.get("times", 0) >= 2:
        await db.locker.update_one(
            {"_id": locker["_id"]},
            {"$set": {"isLocked": False, "times": 0, "code": ""}},
        )
        await db.locker_history.insert_one(
            {
                "lockerId": str(locker["_id"]),
                "lockerNumber": locker["number"],
                "action": "reset",
                "code": locker["code"],
                "timestamp": now_vn,
            }
        )
        updated = await db.locker.find_one({"_id": locker["_id"]})
        updated["_id"] = str(updated["_id"])
        return {"message": "Locker reset after 2 unlocks", "locker": updated}

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
    background_tasks.add_task(auto_lock_locker, locker["_id"], 10)

    updated_locker = await db.locker.find_one({"_id": locker["_id"]})
    updated_locker["_id"] = str(updated_locker["_id"])

    return {"message": "Locker unlocked for 10 seconds", "locker": updated_locker}
