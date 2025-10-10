#!/usr/bin/env python3
import json
import sys

# Read the large file and extract diverse records
records = []
seen_structures = set()
target_count = 100

with open("/Users/jkuefler/src/xapis/scripts/bablic.json", "r") as f:
    for i, line in enumerate(f):
        if len(records) >= target_count:
            break
        try:
            doc = json.loads(line.strip())
            
            # Create a signature of the document structure
            def get_structure(obj, depth=0):
                if depth > 3:
                    return "..."
                if isinstance(obj, dict):
                    return "{"+",".join(sorted(k+":"+get_structure(v, depth+1) for k,v in obj.items()))+"}"
                elif isinstance(obj, list):
                    return "[array]"
                elif obj is None:
                    return "null"
                else:
                    return type(obj).__name__
            
            structure = get_structure(doc)[:500]  # Limit structure string length
            
            # Prioritize records with interesting features
            priority = 0
            doc_str = json.dumps(doc)
            
            # Check for interesting patterns
            if "glossary" in doc and doc.get("glossary"):
                priority += 10
            if "usage_" in doc_str:
                priority += 5
            if "seo_keywords" in doc_str or "seo-keywords" in doc_str:
                priority += 5
            if "sensitive" in doc_str.lower():
                priority += 8
            if "-" in doc_str:
                priority += 2
            if "$" in doc_str:
                priority += 3
            if any(c in doc_str for c in ["4fec1e1a56d5ca0100000050", "4ff17dd746c9900100000059"]):
                priority += 10
            if "notification_types" in doc_str:
                priority += 5
            if "external_tokens" in doc:
                priority += 3
            if "channels" in doc:
                priority += 3
            if "tags" in doc:
                priority += 2
            if "regex_rules" in doc and doc.get("regex_rules"):
                priority += 5
            if "dynamic_urls" in doc and doc.get("dynamic_urls"):
                priority += 5
                
            # Add record if it has interesting features or unique structure
            if priority > 0 or structure not in seen_structures:
                records.append((priority, doc))
                seen_structures.add(structure)
                
        except json.JSONDecodeError:
            continue
        except Exception as e:
            continue
    
    # If we need more records, add some from different parts of the file
    if len(records) < target_count:
        f.seek(0)
        lines = f.readlines()
        # Sample from different parts of the file
        indices = [0, 10, 50, 100, 500, 1000, 5000, 10000, 50000, 100000, 150000, 200000, 250000]
        for idx in indices:
            if idx < len(lines) and len(records) < target_count:
                try:
                    doc = json.loads(lines[idx].strip())
                    records.append((0, doc))
                except:
                    pass

# Sort by priority and take top records
records.sort(key=lambda x: -x[0])
final_records = [doc for _, doc in records[:target_count]]

# Write to sites.json
with open("sites.json", "w") as out:
    for doc in final_records:
        out.write(json.dumps(doc) + "\n")

print(f"Created sites.json with {len(final_records)} records")
