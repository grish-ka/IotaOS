#!/bin/bash

#
# IotaOS - sync_tasks.sh
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#

PROJECT_NAME="IotaOS"

echo "--- $PROJECT_NAME Project Sync ---"

# 1. Clean up Taskwarrior duplicates first
# (Optional: run 'task delete project:IotaOS' manually if it's too messy)

grep -rE "(\/\/|#|/\*) (TODO|READY|DOING|DONE):" src/ | while read -r line; do
    FILE=$(echo "$line" | cut -d: -f1)
    LNUM=$(echo "$line" | cut -d: -f2)
    STATUS=$(echo "$line" | grep -oE "(TODO|READY|DOING|DONE)")
    TEXT=$(echo "$line" | sed -E "s/.*$STATUS: (.*)/\1/" | sed 's/\*\/$//')

    # 2. Check if already tagged with an ID (#123)
    if [[ "$line" =~ \(#([0-9]+)\) ]]; then
        echo "Skipping existing task in $FILE"
        continue
    fi

    # 3. Prevent Duplicate by Description
    if task project:$PROJECT_NAME status:pending export | grep -q "$TEXT"; then
        echo "Task already in Taskwarrior. skipping."
        continue
    fi

    # 4. Create GH Issue (Removed labels to avoid 'label not found' error)
    echo "Creating new $STATUS: $TEXT"
    GH_OUT=$(gh issue create --title "[$STATUS] $TEXT" --body "Auto-sync from $FILE:$LNUM")
    
    if [ $? -eq 0 ]; then
        ISSUE_ID=$(echo "$GH_OUT" | grep -oE "[0-9]+$")
        
        # 5. Add to Taskwarrior
        task add project:$PROJECT_NAME githubid:"$ISSUE_ID" +$STATUS "$TEXT"
        
        # 6. FIXED SED: Use double quotes and clear delimiters
        sed -i "${LNUM}s/$STATUS:/$STATUS(#$ISSUE_ID):/" "$FILE"
        echo "  > Tagged $FILE with #$ISSUE_ID"
    fi
done