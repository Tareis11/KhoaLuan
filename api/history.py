from db import db
from fastapi import APIRouter, Request
from fastapi.responses import JSONResponse
from datetime import datetime


router = APIRouter()


@router.get("/api/history")
async def get_history(request: Request):
    locker_number = request.query_params.get("lockerNumber")
    query = {}
    if locker_number:
        try:
            query["lockerNumber"] = int(locker_number)
        except ValueError:
            return JSONResponse(
                content={"error": "Invalid lockerNumber"}, status_code=400
            )

    history = await db.locker_history.find(query).sort("timestamp", -1).to_list(1000)
    for item in history:
        item["_id"] = str(item["_id"])
        if isinstance(item["timestamp"], datetime):
            item["timestamp"] = item["timestamp"].isoformat()
    return {"history": history}
