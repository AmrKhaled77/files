#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <string.h>
#include <cctype>

using namespace std;

short DocCountID = 0;
short appointmentCountID = 0;

// -------------------- Files --------------------
const string DOCTOR_FILE = "Doctors.dat";
const string APPOINTMENT_FILE = "Appointments.dat";
const string DOCTOR_INDEX_FILE = "DoctorIndex.ind";
const string APPOINTMENT_INDEX_FILE = "AppointmentIndex.ind";
const string APPOINTMENT_SECONDRY_INDEX_FILE = "AppointmentSECIndex.ind";
const string DOCTOR_SECONDRY_INDEX_FILE = "DoctorSECIndex.ind";
const string LL_DOCTOR_FILE = "LLIndexForDoctorName.txt";
const string LL_APPOINTMENT_FILE = "LLIndexForAppointments.txt";

// -------------------- Helpers --------------------
int readIntAt(fstream &f, long long pos) {
    f.seekg(pos);
    int v = 0;
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

void writeIntAt(fstream &f, long long pos, int v) {
    f.seekp(pos);
    f.write(reinterpret_cast<char*>(&v), sizeof(v));
    f.flush();
}

long long readLongAt(fstream &f, long long pos) {
    f.seekg(pos);
    long long v = -1;
    f.read(reinterpret_cast<char*>(&v), sizeof(v));
    return v;
}

void writeLongAt(fstream &f, long long pos, long long v) {
    f.seekp(pos);
    f.write(reinterpret_cast<char*>(&v), sizeof(v));
    f.flush();
}

// Ensure a data file (Appointments.dat / Doctors.dat) exists and has avail-head
void ensureDataFile(const string &fname) {
    fstream f(fname, ios::in | ios::out | ios::binary);
    if (!f.is_open()) {
        ofstream out(fname, ios::binary);
        long long head = -1;
        out.write(reinterpret_cast<char*>(&head), sizeof(head));
        out.close();
        return;
    }
    f.seekg(0, ios::end);
    if (f.tellg() < static_cast<streampos>(sizeof(long long))) {
        f.seekp(0);
        long long head = -1;
        f.write(reinterpret_cast<char*>(&head), sizeof(head));
    }
    f.close();
}

// Ensure an index / LL file exists (no header)
void ensureIndexFile(const string &fname) {
    fstream f(fname, ios::in | ios::out | ios::binary);
    if (!f.is_open()) {
        ofstream out(fname, ios::binary);
        out.close();
        return;
    }
    f.close();
}

// Split fields by '|'
vector<string> splitFields(const string &s) {
    vector<string> out;
    string cur;
    for (char c : s) {
        if (c == '|') {
            out.push_back(cur);
            cur.clear();
        }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}
short loadCountID(const string &indexFile) {
    fstream f(indexFile, ios::binary | ios::in);
    if (!f.is_open()) {
        return 0;
    }

    f.seekg(0, ios::end);
    long long bytes = f.tellg();
    f.close();

    if (bytes == 0) return 0;

    short count = static_cast<short>(bytes / (sizeof(int) + sizeof(short)));
    return count;
}
// Utility function to trim strings
void trim(string &str) {
    str.erase(0, str.find_first_not_of(" \t\n\r\f\v"));
    str.erase(str.find_last_not_of(" \t\n\r\f\v") + 1);
}

// Function to convert string to lowercase
string toLower(const string& str) {
    string result = str;
    transform(result.begin(), result.end(), result.begin(),
              [](unsigned char c) { return tolower(c); });
    return result;
}

// -------------------- Index --------------------
struct RecIndex {
    long long recOffset;
    int recLen;
    string id;
    string body;
};

// insertInPrimaryIndex: stores pairs (int id, short offset) sorted by id
void insertInPrimaryIndex(const string& id, long long offset, const string& indexFileName, short &CountID) {
    int x = stoi(id);

    fstream prim(indexFileName, ios::in | ios::out | ios::binary);
    if (!prim.is_open()) {
        prim.open(indexFileName, ios::out | ios::binary);
        prim.close();
        prim.open(indexFileName, ios::in | ios::out | ios::binary);
    }

    if (CountID == 0) {
        // First insertion
        prim.seekp(0);
        prim.write(reinterpret_cast<const char*>(&x), sizeof(x));
        short shortOffset = static_cast<short>(offset);
        prim.write(reinterpret_cast<const char*>(&shortOffset), sizeof(shortOffset));
        CountID++;
        prim.close();
        return;
    }

    // Read all entries
    vector<pair<int, short>> entries;
    for (int i = 0; i < CountID; i++) {
        int id_val;
        short offset_val;
        prim.seekg(i * (sizeof(int) + sizeof(short)));
        prim.read(reinterpret_cast<char*>(&id_val), sizeof(id_val));
        prim.read(reinterpret_cast<char*>(&offset_val), sizeof(offset_val));
        entries.push_back({id_val, offset_val});
    }
    prim.close();

    // Find insertion position
    auto it = entries.begin();
    while (it != entries.end() && it->first < x) {
        it++;
    }

    // Insert at correct position
    short shortOffset = static_cast<short>(offset);
    entries.insert(it, {x, shortOffset});
    CountID++;

    // Write back all entries
    prim.open(indexFileName, ios::out | ios::binary | ios::trunc);
    for (const auto& entry : entries) {
        prim.write(reinterpret_cast<const char*>(&entry.first), sizeof(entry.first));
        prim.write(reinterpret_cast<const char*>(&entry.second), sizeof(entry.second));
    }
    prim.close();
}

short BinarySearchId(int ID1, const string& indexName, short CountID) {
    fstream prim(indexName, ios::in | ios::binary);
    if (!prim.is_open() || CountID == 0) {
        prim.close();
        return -1;
    }

    int left = 0;
    int right = CountID - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        prim.seekg(mid * (sizeof(int) + sizeof(short)));

        int currentID;
        prim.read(reinterpret_cast<char*>(&currentID), sizeof(currentID));

        if (currentID == ID1) {
            short offset;
            prim.read(reinterpret_cast<char*>(&offset), sizeof(offset));
            prim.close();
            return offset;
        }
        else if (currentID < ID1) {
            left = mid + 1;
        }
        else {
            right = mid - 1;
        }
    }

    prim.close();
    return -1;
}

void Deleteprimary(int ID1, const string& indexName, short &CountID) {
    fstream prim(indexName, ios::in | ios::binary | ios::out);
    if (!prim.is_open() || CountID == 0) {
        prim.close();
        return;
    }

    // Find the ID
    for (int i = 0; i < CountID; i++) {
        int id_val;
        prim.seekg(i * (sizeof(int) + sizeof(short)));
        prim.read(reinterpret_cast<char*>(&id_val), sizeof(id_val));

        if (id_val == ID1) {
            // Shift remaining entries
            for (int j = i + 1; j < CountID; j++) {
                int next_id;
                short next_offset;
                prim.seekg(j * (sizeof(int) + sizeof(short)));
                prim.read(reinterpret_cast<char*>(&next_id), sizeof(next_id));
                prim.read(reinterpret_cast<char*>(&next_offset), sizeof(next_offset));

                prim.seekp((j - 1) * (sizeof(int) + sizeof(short)));
                prim.write(reinterpret_cast<const char*>(&next_id), sizeof(next_id));
                prim.write(reinterpret_cast<const char*>(&next_offset), sizeof(next_offset));
            }

            prim.close();

            // Truncate file (platform-dependent approach: rewrite up to CountID-1)
            fstream trunc(indexName, ios::in | ios::out | ios::binary);
            trunc.close();

            CountID--;
            return;
        }
    }
    prim.close();
}

// loadIndex: scan data file (reads avail-head then records)
void loadIndex(const string &fname, unordered_map<string, RecIndex> &indexMap) {
    indexMap.clear();
    ensureDataFile(fname);

    fstream f(fname, ios::in | ios::binary);
    if (!f.is_open()) {
        return;
    }

    // Read avail list head
    long long avail;
    f.read(reinterpret_cast<char*>(&avail), sizeof(avail));

    // Get file size
    f.seekg(0, ios::end);
    long long fileSize = f.tellg();

    // Start reading records
    long long pos = sizeof(avail);

    while (pos < fileSize) {
        f.seekg(pos);

        // Read record length
        int rec_len;
        if (!f.read(reinterpret_cast<char*>(&rec_len), sizeof(rec_len))) {
            break;
        }

        // Check for valid length
        if (rec_len <= 0 || rec_len > 10000) {
            pos += sizeof(rec_len) + max(0, rec_len);
            continue;
        }

        // Check if we have enough data
        if (pos + sizeof(rec_len) + rec_len > fileSize) {
            break;
        }

        // Read record body
        vector<char> buf(rec_len);
        if (!f.read(buf.data(), rec_len)) {
            break;
        }

        // Check if not deleted
        if (!buf.empty() && buf[0] != '*') {
            string body(buf.begin(), buf.end());
            vector<string> fields = splitFields(body);
            if (!fields.empty()) {
                RecIndex r;
                r.recOffset = pos;
                r.recLen = rec_len;
                r.id = fields[0];
                r.body = body;
                indexMap[r.id] = r;
            }
        }

        // Move to next record
        pos += sizeof(rec_len) + rec_len;
    }

    f.close();
}

// NEW: loadPrimaryIndex - read (id, offset) entries from primary index file and fill indexMap
void loadPrimaryIndex(const string &indexFile, const string &dataFile, unordered_map<string, RecIndex> &indexMap) {
    indexMap.clear();

    // Ensure files exist
    ensureIndexFile(indexFile);
    ensureDataFile(dataFile);

    fstream idx(indexFile, ios::in | ios::binary);
    if (!idx.is_open()) return;

    // determine number of entries
    idx.seekg(0, ios::end);
    long long bytes = idx.tellg();
    if (bytes == 0) {
        idx.close();
        return;
    }
    short entries = static_cast<short>(bytes / (sizeof(int) + sizeof(short)));
    idx.seekg(0);

    fstream data(dataFile, ios::in | ios::binary);
    if (!data.is_open()) {
        idx.close();
        return;
    }

    for (int i = 0; i < entries; ++i) {
        int id_val;
        short offset_short;
        idx.read(reinterpret_cast<char*>(&id_val), sizeof(id_val));
        idx.read(reinterpret_cast<char*>(&offset_short), sizeof(offset_short));
        long long offset = static_cast<long long>(offset_short);

        if (offset < 0) continue;

        // Seek to record in data file
        data.seekg(offset);
        int rec_len;
        if (!data.read(reinterpret_cast<char*>(&rec_len), sizeof(rec_len))) continue;
        if (rec_len <= 0 || rec_len > 10000) continue;
        vector<char> buf(rec_len);
        if (!data.read(buf.data(), rec_len)) continue;
        // if deleted skip
        if (!buf.empty() && buf[0] == '*') continue;

        string body(buf.begin(), buf.end());
        RecIndex r;
        r.recOffset = offset;
        r.recLen = rec_len;
        r.id = to_string(id_val);
        r.body = body;
        indexMap[r.id] = r;
    }

    data.close();
    idx.close();
}

// Secondry Index

struct LLNode {
    char docID[10];
    short next; // offset in LL file (in nodes, not bytes)
};

struct SecIndexRecord {
    char name[20]; // padded with '\0'
    short headLL;  // offset/index of first node in LL file
};

void insertSecondaryDoctorName(const string &name, const string &docID) {
    fstream sec(DOCTOR_SECONDRY_INDEX_FILE, ios::binary | ios::in | ios::out);
    fstream ll(LL_DOCTOR_FILE, ios::binary | ios::in | ios::out);

    if (!sec.is_open()) {
        sec.open(DOCTOR_SECONDRY_INDEX_FILE, ios::binary | ios::out | ios::app);
        sec.close();
        sec.open(DOCTOR_SECONDRY_INDEX_FILE, ios::binary | ios::in | ios::out);
    }

    if (!ll.is_open()) {
        ll.open(LL_DOCTOR_FILE, ios::binary | ios::out | ios::app);
        ll.close();
        ll.open(LL_DOCTOR_FILE, ios::binary | ios::in | ios::out);
    }

    // --- Step 1: Append new node to linked list ---
    LLNode node;
    strncpy(node.docID, docID.c_str(), 9);
    node.docID[9] = '\0';
    node.next = -1;

    ll.seekp(0, ios::end);
    long long llEnd = ll.tellp();
    short nodeOffset;

    if (llEnd == 0) {
        nodeOffset = 0;
    } else {
        nodeOffset = static_cast<short>(llEnd / sizeof(LLNode));
    }

    ll.write(reinterpret_cast<const char*>(&node), sizeof(node));
    ll.flush();

    // --- Step 2: Read all secondary index into memory ---
    sec.seekg(0, ios::end);
    long long numRecords = sec.tellg() / sizeof(SecIndexRecord);
    vector<SecIndexRecord> records;
    sec.seekg(0);
    for (long long i = 0; i < numRecords; i++) {
        SecIndexRecord rec;
        sec.read(reinterpret_cast<char*>(&rec), sizeof(rec));
        records.push_back(rec);
    }

    // --- Step 3: Check if name exists ---
    bool found = false;
    short headLL = -1;
    for (auto &rec : records) {
        if (strcmp(rec.name, name.c_str()) == 0) {
            found = true;
            headLL = rec.headLL;
            break;
        }
    }

    if (found) {
        // --- Step 4: Append to existing linked list ---
        short curr = headLL;
        LLNode temp;
        while (curr != -1) {
            ll.seekg(curr * sizeof(LLNode));
            ll.read(reinterpret_cast<char*>(&temp), sizeof(temp));
            if (temp.next == -1) break;
            curr = temp.next;
        }
        temp.next = nodeOffset;
        ll.seekp(curr * sizeof(LLNode));
        ll.write(reinterpret_cast<const char*>(&temp), sizeof(temp));
    } else {
        // --- Step 5: Insert new record in sorted order ---
        SecIndexRecord newRec;
        strncpy(newRec.name, name.c_str(), 19);
        newRec.name[19] = '\0';
        newRec.headLL = nodeOffset;

        // Find insertion position
        auto it = records.begin();
        while (it != records.end() && strcmp(it->name, name.c_str()) < 0) {
            it++;
        }

        // Insert in vector
        records.insert(it, newRec);

        // --- Step 6: Rewrite the entire secondary index file ---
        sec.close();
        sec.open(DOCTOR_SECONDRY_INDEX_FILE, ios::binary | ios::out | ios::trunc);
        for (auto &rec : records) {
            sec.write(reinterpret_cast<const char*>(&rec), sizeof(rec));
        }
    }

    ll.close();
    sec.close();
}

vector<string> searchDoctorByName(const string &name) {
    vector<string> results;

    fstream sec(DOCTOR_SECONDRY_INDEX_FILE, ios::binary | ios::in);
    if (!sec.is_open()) {
        return results;
    }

    fstream ll(LL_DOCTOR_FILE, ios::binary | ios::in);
    if (!ll.is_open()) {
        sec.close();
        return results;
    }

    // Binary search for name
    sec.seekg(0, ios::end);
    long long numRecords = sec.tellg() / sizeof(SecIndexRecord);
    long long first = 0, last = numRecords - 1, mid;
    bool found = false;
    short headLL = -1;

    while (first <= last) {
        mid = (first + last) / 2;
        SecIndexRecord rec;
        sec.seekg(mid * sizeof(SecIndexRecord));
        sec.read(reinterpret_cast<char*>(&rec), sizeof(rec));
        int cmp = strcmp(rec.name, name.c_str());
        if (cmp == 0) {
            found = true;
            headLL = rec.headLL;
            break;
        }
        else if (cmp > 0) last = mid - 1;
        else first = mid + 1;
    }

    if (!found) {
        sec.close();
        ll.close();
        return results;
    }

    // Traverse LL
    short curr = headLL;
    LLNode node;
    while (curr != -1) {
        ll.seekg(curr * sizeof(LLNode));
        ll.read(reinterpret_cast<char*>(&node), sizeof(node));
        results.push_back(string(node.docID));
        curr = node.next;
    }

    ll.close();
    sec.close();
    return results;
}

void insertSecondaryAppointment(const string &doctorID, const string &appID) {

    // Ensure files exist
    ensureIndexFile(APPOINTMENT_SECONDRY_INDEX_FILE);
    ensureIndexFile(LL_APPOINTMENT_FILE);

    fstream sec(APPOINTMENT_SECONDRY_INDEX_FILE, ios::binary | ios::in | ios::out);
    fstream ll(LL_APPOINTMENT_FILE, ios::binary | ios::in | ios::out);

    // --- Step 1: Append new LL node ---
    LLNode node;
    strncpy(node.docID, appID.c_str(), 9);
    node.docID[9] = '\0';
    node.next = -1;

    ll.seekp(0, ios::end);
    long long llSizeBytes = ll.tellp();
    short newNode = (llSizeBytes == 0 ? 0 : llSizeBytes / sizeof(LLNode));

    ll.write(reinterpret_cast<const char*>(&node), sizeof(node));
    ll.flush();

    // --- Step 2: Load secondary index ---
    sec.seekg(0, ios::end);
    long long recCount = sec.tellg() / sizeof(SecIndexRecord);
    vector<SecIndexRecord> records(recCount);

    sec.seekg(0);
    for (int i = 0; i < recCount; i++)
        sec.read(reinterpret_cast<char*>(&records[i]), sizeof(SecIndexRecord));

    // --- Step 3: Check if doctorID exists ---
    int existPos = -1;
    for (int i = 0; i < recCount; i++) {
        if (strcmp(records[i].name, doctorID.c_str()) == 0) {
            existPos = i;
            break;
        }
    }

    if (existPos != -1) {
        // Append to existing LL
        short head = records[existPos].headLL;

        // Traverse until the last node
        short curr = head;
        LLNode temp;
        while (true) {
            ll.seekg(curr * sizeof(LLNode));
            ll.read(reinterpret_cast<char*>(&temp), sizeof(temp));

            if (temp.next == -1) break;
            curr = temp.next;
        }

        // Link new node
        temp.next = newNode;
        ll.seekp(curr * sizeof(LLNode));
        ll.write(reinterpret_cast<const char*>(&temp), sizeof(temp));
        ll.flush();
    }
    else {
        // Insert new sorted record
        SecIndexRecord rec;
        strncpy(rec.name, doctorID.c_str(), 19);
        rec.name[19] = '\0';
        rec.headLL = newNode;

        // Insert sorted
        auto it = records.begin();
        while (it != records.end() && strcmp(it->name, rec.name) < 0)
            it++;

        records.insert(it, rec);

        // Rewrite secondary index file
        sec.close();
        sec.open(APPOINTMENT_SECONDRY_INDEX_FILE, ios::binary | ios::out | ios::trunc);
        for (auto &r : records)
            sec.write(reinterpret_cast<const char*>(&r), sizeof(r));
    }

    ll.close();
    sec.close();
}


vector<string> searchAppointmentsByDoctor(const string &doctorID) {
    vector<string> results;

    fstream sec(APPOINTMENT_SECONDRY_INDEX_FILE, ios::binary | ios::in);
    if (!sec.is_open()) return results;

    fstream ll(LL_APPOINTMENT_FILE, ios::binary | ios::in);
    if (!ll.is_open()) {
        sec.close();
        return results;
    }

    // Binary search
    sec.seekg(0, ios::end);
    long long recCount = sec.tellg() / sizeof(SecIndexRecord);
    long long left = 0, right = recCount - 1;

    short head = -1;
    SecIndexRecord rec;

    while (left <= right) {
        long long mid = (left + right) / 2;

        sec.seekg(mid * sizeof(SecIndexRecord));
        sec.read(reinterpret_cast<char*>(&rec), sizeof(rec));

        int cmp = strcmp(rec.name, doctorID.c_str());
        if (cmp == 0) {
            head = rec.headLL;
            break;
        } else if (cmp < 0) {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    if (head == -1) {
        sec.close();
        ll.close();
        return results;
    }

    // Traverse LL (no duplicates)
    short curr = head;
    LLNode node;

    while (curr != -1) {
        ll.seekg(curr * sizeof(LLNode));
        ll.read(reinterpret_cast<char*>(&node), sizeof(node));

        results.push_back(string(node.docID));
        curr = node.next;
    }

    ll.close();
    sec.close();
    return results;
}

// -------------------- AVAIL list --------------------
long long readAvailHead(const string &fname) {
    ensureDataFile(fname);
    ifstream fin(fname, ios::binary);
    long long head = -1;
    fin.read(reinterpret_cast<char*>(&head), sizeof(head));
    fin.close();
    return head;
}

void writeAvailHead(const string &fname, long long head) {
    fstream f(fname, ios::in | ios::out | ios::binary);
    writeLongAt(f, 0, head);
    f.close();
}

long long readNextDeletedAt(fstream &f, long long recOffset) {
    int rec_len = readIntAt(f, recOffset);
    long long nextPtr = -1;
    long long bodyPos = recOffset + sizeof(rec_len);
    f.seekg(bodyPos + 1); // skip '*'
    f.read(reinterpret_cast<char*>(&nextPtr), sizeof(nextPtr));
    return nextPtr;
}

void writeDeletedAt(fstream &f, long long recOffset, long long nextPtr) {
    int rec_len = readIntAt(f, recOffset);
    long long bodyPos = recOffset + sizeof(rec_len);
    f.seekp(bodyPos);
    char star = '*';
    f.write(&star, 1);
    f.write(reinterpret_cast<const char*>(&nextPtr), sizeof(nextPtr));
    int pad = rec_len - (1 + sizeof(nextPtr));
    if (pad > 0) {
        string zeros(pad, ' ');
        f.write(zeros.data(), pad);
    }
    f.flush();
}

void writeActiveBodyAt(fstream &f, long long recOffset, const string &body) {
    int rec_len = readIntAt(f, recOffset);
    if (static_cast<int>(body.size()) > rec_len) {
        throw runtime_error("body too big for slot");
    }

    long long bodyPos = recOffset + sizeof(rec_len);
    f.seekp(bodyPos);
    f.write(body.data(), body.size());
    int pad = rec_len - static_cast<int>(body.size());
    if (pad > 0) {
        string spaces(pad, ' ');
        f.write(spaces.data(), pad);
    }
    f.flush();
}

// FIXED appendRecord: ensures data-file header exists then appends record
long long appendRecord(fstream &f, const string &body) {
    f.seekp(0, ios::end);
    long long endPos = f.tellp();

    // If file is empty, write avail-head first
    if (endPos == 0) {
        long long head = -1;
        f.write(reinterpret_cast<const char*>(&head), sizeof(head));
        endPos = sizeof(head);
    }

    long long recOffset = endPos;
    int rec_len = static_cast<int>(body.size());
    f.write(reinterpret_cast<const char*>(&rec_len), sizeof(rec_len));
    f.write(body.data(), body.size());
    f.flush();
    return recOffset;
}

long long findFitInAvailAndRemove(const string &fname, int neededLen) {
    fstream f(fname, ios::in | ios::out | ios::binary);
    long long head = readLongAt(f, 0);
    long long prev = -1;
    long long cur = head;

    while (cur != -1) {
        int rec_len = readIntAt(f, cur);
        if (rec_len >= neededLen) {
            long long nextPtr = readNextDeletedAt(f, cur);
            if (prev == -1) {
                writeLongAt(f, 0, nextPtr);
            } else {
                long long prevBodyPos = prev + sizeof(int);
                f.seekp(prevBodyPos + 1);
                f.write(reinterpret_cast<const char*>(&nextPtr), sizeof(nextPtr));
                f.flush();
            }
            f.close();
            return cur;
        }
        prev = cur;
        cur = readNextDeletedAt(f, cur);
    }
    f.close();
    return -1;
}

// -------------------- Doctor operations --------------------
void addDoctor() {
    unordered_map<string, RecIndex> idx;
    // Load primary index -> faster and authoritative
    loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, idx);
    long long recOffset;

    string id, name, address;
    cout << "Enter Doctor ID: ";
    cin >> id;

    if (BinarySearchId(stoi(id), DOCTOR_INDEX_FILE, DocCountID) != -1) {
        cout << "Doctor already exists!\n";
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "Enter Name: ";
    getline(cin, name);
    cout << "Enter Address: ";
    getline(cin, address);

    string body = id + "|" + name + "|" + address;
    int needed = static_cast<int>(body.size());

    ensureDataFile(DOCTOR_FILE);
    long long slot = findFitInAvailAndRemove(DOCTOR_FILE, needed);
    fstream f(DOCTOR_FILE, ios::in | ios::out | ios::binary);

    if (slot != -1) {
        writeActiveBodyAt(f, slot, body);
        recOffset = slot;
        cout << "Doctor added (reused deleted slot)\n";
    } else {
        recOffset = appendRecord(f, body);
        cout << "Doctor appended at file end\n";
    }
    f.close();

    // Update primary index
    insertInPrimaryIndex(id, recOffset, DOCTOR_INDEX_FILE, DocCountID);

    // Update secondary index
    insertSecondaryDoctorName(name, id);
}

void deleteDoctor() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, idx);

    string id;
    cout << "Enter Doctor ID to delete: ";
    cin >> id;

    auto it = idx.find(id);
    if (it == idx.end()) {
        cout << "Doctor not found!\n";
        return;
    }

    // Perform the logical deletion
    fstream f(DOCTOR_FILE, ios::in | ios::out | ios::binary);
    long long head = readLongAt(f, 0);
    writeDeletedAt(f, it->second.recOffset, head);
    writeLongAt(f, 0, it->second.recOffset);
    f.close();

    cout << "Doctor logically deleted\n";

    // Delete from primary index
    int num = stoi(id);
    Deleteprimary(num, DOCTOR_INDEX_FILE, DocCountID);
}

void updateDoctorName() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, idx);

    string id;
    cout << "Enter Doctor ID: ";
    cin >> id;

    auto it = idx.find(id);
    if (it == idx.end()) {
        cout << "Doctor not found!\n";
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string newName;
    cout << "Enter new Name: ";
    getline(cin, newName);

    vector<string> fields = splitFields(it->second.body);
    string newBody = id + "|" + newName + "|" + (fields.size() > 2 ? fields[2] : "");

    if (static_cast<int>(newBody.size()) > it->second.recLen) {
        cout << "New data too long\n";
        return;
    }

    fstream f(DOCTOR_FILE, ios::in | ios::out | ios::binary);
    writeActiveBodyAt(f, it->second.recOffset, newBody);
    f.close();

    cout << "Doctor name updated\n";
}

void printDoctorInfo() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, idx);

    string id;
    cout << "Enter Doctor ID: ";
    cin >> id;

    auto it = idx.find(id);
    if (it == idx.end()) {
        cout << "Doctor not found!\n";
        return;
    }

    vector<string> fields = splitFields(it->second.body);
    cout << "ID: " << fields[0] << "\n";
    cout << "Name: " << (fields.size() > 1 ? fields[1] : "") << "\n";
    cout << "Address: " << (fields.size() > 2 ? fields[2] : "") << "\n";
}

void listAllDoctors() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, idx);

    if (idx.empty()) {
        cout << "No doctors found.\n";
        return;
    }

    cout << "=== Doctors ===\n";
    for (auto &p: idx) {
        vector<string> f = splitFields(p.second.body);
        cout << p.second.id << " | "
             << (f.size() > 1 ? f[1] : "") << " | "
             << (f.size() > 2 ? f[2] : "") << "\n";
    }
}

short searchForDocID(int id) {
    return BinarySearchId(id, DOCTOR_INDEX_FILE, DocCountID);
}

// -------------------- Appointment operations --------------------
void addAppointment() {
    // load doctor primary index (to confirm doctor exists quickly)
    unordered_map<string, RecIndex> docIdx;
    loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, docIdx);

    // load appointments primary index map
    unordered_map<string, RecIndex> apIdx;
    loadPrimaryIndex(APPOINTMENT_INDEX_FILE, APPOINTMENT_FILE, apIdx);

    long long recOffset;

    string appID;
    cout << "Enter Appointment ID: ";
    cin >> appID;

    if (BinarySearchId(stoi(appID), APPOINTMENT_INDEX_FILE, appointmentCountID) != -1) {
        cout << "Appointment already exists!\n";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    string doctorID;
    cout << "Enter Doctor ID: ";
    cin >> doctorID;

    // Check if doctor exists using primary index
    if (BinarySearchId(stoi(doctorID), DOCTOR_INDEX_FILE, DocCountID) == -1) {
        cout << "Doctor does not exist!\n";
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string date;
    cout << "Enter Appointment Date: ";
    getline(cin, date);

    string body = appID + "|" + date + "|" + doctorID;
    int needed = static_cast<int>(body.size());

    ensureDataFile(APPOINTMENT_FILE);
    long long slot = findFitInAvailAndRemove(APPOINTMENT_FILE, needed);
    fstream f(APPOINTMENT_FILE, ios::in | ios::out | ios::binary);

    if (slot != -1) {
        writeActiveBodyAt(f, slot, body);
        recOffset = slot;
        cout << "Appointment added (reused deleted slot)\n";
    } else {
        recOffset = appendRecord(f, body);
        cout << "Appointment appended at file end\n";
    }
    f.close();

    // Update primary index
    insertInPrimaryIndex(appID, recOffset, APPOINTMENT_INDEX_FILE, appointmentCountID);

    // Update secondary index
    insertSecondaryAppointment(doctorID, appID);

    cout << "Appointment added successfully!\n";
}

void deleteAppointment() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(APPOINTMENT_INDEX_FILE, APPOINTMENT_FILE, idx);

    string id;
    cout << "Enter Appointment ID: ";
    cin >> id;

    auto it = idx.find(id);
    if (it == idx.end()) {
        cout << "Appointment not found!\n";
        return;
    }

    // Perform the logical deletion
    fstream f(APPOINTMENT_FILE, ios::in | ios::out | ios::binary);
    long long head = readLongAt(f, 0);
    writeDeletedAt(f, it->second.recOffset, head);
    writeLongAt(f, 0, it->second.recOffset);
    f.close();

    // Delete from primary index
    int num = stoi(id);
    Deleteprimary(num, APPOINTMENT_INDEX_FILE, appointmentCountID);

    cout << "Appointment logically deleted\n";
}

void updateAppointmentDate() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(APPOINTMENT_INDEX_FILE, APPOINTMENT_FILE, idx);

    string id;
    cout << "Enter Appointment ID: ";
    cin >> id;

    auto it = idx.find(id);
    if (it == idx.end()) {
        cout << "Appointment not found!\n";
        return;
    }

    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string newDate;
    cout << "Enter new Date: ";
    getline(cin, newDate);

    vector<string> flds = splitFields(it->second.body);
    string newBody = id + "|" + newDate + "|" + (flds.size() > 2 ? flds[2] : "");

    if (static_cast<int>(newBody.size()) > it->second.recLen) {
        cout << "New data too long\n";
        return;
    }

    fstream f(APPOINTMENT_FILE, ios::in | ios::out | ios::binary);
    writeActiveBodyAt(f, it->second.recOffset, newBody);
    f.close();

    cout << "Appointment updated\n";
}

void printAppointmentInfo() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(APPOINTMENT_INDEX_FILE, APPOINTMENT_FILE, idx);

    string id;
    cout << "Enter Appointment ID: ";
    cin >> id;

    auto it = idx.find(id);
    if (it == idx.end()) {
        cout << "Appointment not found!\n";
        return;
    }

    vector<string> f = splitFields(it->second.body);
    cout << "ID: " << f[0] << "\n";
    cout << "Date: " << (f.size() > 1 ? f[1] : "") << "\n";
    cout << "DoctorID: " << (f.size() > 2 ? f[2] : "") << "\n";
}
void listAllAppointments() {
    unordered_map<string, RecIndex> idx;
    loadPrimaryIndex(APPOINTMENT_INDEX_FILE, APPOINTMENT_FILE, idx);

    if (idx.empty()) {
        cout << "No appointments found.\n";
        return;
    }

    cout << "=== Appointments ===\n";
    for (auto &p: idx) {
        vector<string> f = splitFields(p.second.body);
        cout << p.second.id << " | "
             << (f.size() > 1 ? f[1] : "") << " | "
             << (f.size() > 2 ? f[2] : "") << "\n";
    }
}

short searchForAppointID(int id) {
    return BinarySearchId(id, APPOINTMENT_INDEX_FILE, appointmentCountID);
}

// -------------------- Query Parser --------------------
struct QueryResult {
    vector<string> columns;
    vector<vector<string>> rows;
};

// Helper function to extract value from WHERE clause
string extractValueFromWhereClause(const string& whereClause, const string& key) {
    if (whereClause.empty()) return "";

    string lowerWhere = toLower(whereClause);
    string lowerKey = toLower(key);

    // Look for key pattern
    size_t keyPos = lowerWhere.find(lowerKey);
    if (keyPos == string::npos) {
        return "";
    }

    // Find equals sign after key
    size_t equalsPos = whereClause.find('=', keyPos);
    if (equalsPos == string::npos) {
        return "";
    }

    // Skip whitespace after equals
    size_t valueStart = equalsPos + 1;
    while (valueStart < whereClause.size() && isspace(whereClause[valueStart])) {
        valueStart++;
    }

    // Check for quotes
    if (valueStart < whereClause.size() && whereClause[valueStart] == '\'') {
        size_t quoteEnd = whereClause.find('\'', valueStart + 1);
        if (quoteEnd == string::npos) {
            return "";
        }
        return whereClause.substr(valueStart + 1, quoteEnd - valueStart - 1);
    }

    // Read until space or semicolon
    size_t valueEnd = valueStart;
    while (valueEnd < whereClause.size() && !isspace(whereClause[valueEnd]) && whereClause[valueEnd] != ';')
        valueEnd++;

    return whereClause.substr(valueStart, valueEnd - valueStart);
}

// Function to identify what type of WHERE clause we have
string identifyWhereClauseType(const string& whereClause) {
    if (whereClause.empty()) return "none";

    string lowerWhere = toLower(whereClause);

    if (lowerWhere.find("appointment id") != string::npos ||
        (lowerWhere.find("id") != string::npos && lowerWhere.find("doctor") == string::npos)) {
        return "appointment_id";
    } else if (lowerWhere.find("doctor id") != string::npos) {
        return "doctor_id";
    } else if (lowerWhere.find("doctor name") != string::npos ||
               (lowerWhere.find("name") != string::npos && lowerWhere.find("doctor") != string::npos)) {
        return "doctor_name";
    }

    return "unknown";
}

QueryResult executeAppointmentQuery(const string& selectClause, const string& whereClause) {
    QueryResult result;

    // Parse SELECT clause
    string lowerSelect = toLower(selectClause);
    if (lowerSelect == "all" || lowerSelect == "*") {
        result.columns = {"AppointmentID", "Date", "DoctorID"};
    } else if (lowerSelect.find("appointment id") != string::npos ||
               lowerSelect.find("id") != string::npos) {
        result.columns = {"AppointmentID"};
    } else if (lowerSelect.find("date") != string::npos) {
        result.columns = {"Date"};
    } else if (lowerSelect.find("doctor id") != string::npos) {
        result.columns = {"DoctorID"};
    } else {
        cout << "Unsupported SELECT clause for Appointments\n";
        return result;
    }

    // Parse WHERE clause
    if (whereClause.empty()) {
        // No WHERE clause - return all records (from primary index for ordered)
        unordered_map<string, RecIndex> idx;
        loadPrimaryIndex(APPOINTMENT_INDEX_FILE, APPOINTMENT_FILE, idx);

        for (auto &p: idx) {
            vector<string> fields = splitFields(p.second.body);
            if (fields.size() < 2) continue;

            vector<string> row;
            if (result.columns.size() == 3) {
                row = {fields[0],
                      fields.size() > 1 ? fields[1] : "",
                      fields.size() > 2 ? fields[2] : ""};
            } else if (result.columns[0] == "AppointmentID") {
                row = {fields[0]};
            } else if (result.columns[0] == "Date") {
                row = {fields.size() > 1 ? fields[1] : ""};
            } else if (result.columns[0] == "DoctorID") {
                row = {fields.size() > 2 ? fields[2] : ""};
            }
            result.rows.push_back(row);
        }
    } else {
        // Identify what type of WHERE clause we have
        string whereType = identifyWhereClauseType(whereClause);

        if (whereType == "appointment_id") {
            // Use primary index (Appointment ID)
            string appointmentID = extractValueFromWhereClause(whereClause, "appointment id");
            if (appointmentID.empty()) {
                appointmentID = extractValueFromWhereClause(whereClause, "id");
            }

            if (!appointmentID.empty()) {
                try {
                    int id = stoi(appointmentID);
                    short offset = searchForAppointID(id);

                    if (offset != -1) {
                        // Read appointment record
                        fstream f(APPOINTMENT_FILE, ios::in | ios::binary);
                        f.seekg(offset);
                        int rec_len;
                        f.read(reinterpret_cast<char*>(&rec_len), sizeof(rec_len));
                        vector<char> buf(rec_len);
                        f.read(buf.data(), rec_len);
                        f.close();

                        string body(buf.begin(), buf.end());
                        vector<string> fields = splitFields(body);

                        if (result.columns.size() == 3) {
                            result.rows.push_back({fields[0],
                                                  fields.size() > 1 ? fields[1] : "",
                                                  fields.size() > 2 ? fields[2] : ""});
                        } else if (result.columns[0] == "AppointmentID") {
                            result.rows.push_back({fields[0]});
                        } else if (result.columns[0] == "Date") {
                            result.rows.push_back({fields.size() > 1 ? fields[1] : ""});
                        } else if (result.columns[0] == "DoctorID") {
                            result.rows.push_back({fields.size() > 2 ? fields[2] : ""});
                        }
                    }
                } catch (...) {
                    // Invalid ID format
                }
            }
        } else if (whereType == "doctor_id") {
            // Use secondary index (Doctor ID)
            string doctorID = extractValueFromWhereClause(whereClause, "doctor id");

            if (!doctorID.empty()) {
                // Search using secondary index
                vector<string> appointmentIDs = searchAppointmentsByDoctor(doctorID);

                if (!appointmentIDs.empty()) {
                    // Load index to get full records
                    unordered_map<string, RecIndex> idx;
                    loadPrimaryIndex(APPOINTMENT_INDEX_FILE, APPOINTMENT_FILE, idx);

                    for (const string& id : appointmentIDs) {
                        auto it = idx.find(id);
                        if (it != idx.end()) {
                            vector<string> fields = splitFields(it->second.body);

                            if (result.columns.size() == 3) {
                                result.rows.push_back({fields[0],
                                                      fields.size() > 1 ? fields[1] : "",
                                                      fields.size() > 2 ? fields[2] : ""});
                            } else if (result.columns[0] == "AppointmentID") {
                                result.rows.push_back({fields[0]});
                            } else if (result.columns[0] == "Date") {
                                result.rows.push_back({fields.size() > 1 ? fields[1] : ""});
                            } else if (result.columns[0] == "DoctorID") {
                                result.rows.push_back({fields.size() > 2 ? fields[2] : ""});
                            }
                        }
                    }
                }
            }
        } else {
            cout << "Unsupported WHERE clause for Appointments\n";
        }
    }

    return result;
}

QueryResult executeDoctorQuery(const string& selectClause, const string& whereClause) {
    QueryResult result;

    // Parse SELECT clause
    string lowerSelect = toLower(selectClause);
    if (lowerSelect == "all" || lowerSelect == "*") {
        result.columns = {"ID", "Name", "Address"};
    } else if (lowerSelect.find("doctor name") != string::npos ||
               lowerSelect.find("name") != string::npos) {
        result.columns = {"Name"};
    } else if (lowerSelect.find("doctor id") != string::npos ||
               lowerSelect.find("id") != string::npos) {
        result.columns = {"ID"};
    } else if (lowerSelect.find("address") != string::npos) {
        result.columns = {"Address"};
    } else {
        cout << "Unsupported SELECT clause for Doctors\n";
        return result;
    }

    // Parse WHERE clause
    if (whereClause.empty()) {
        // No WHERE clause - return all records
        unordered_map<string, RecIndex> idx;
        loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, idx);

        for (auto &p: idx) {
            vector<string> fields = splitFields(p.second.body);
            if (fields.size() < 2) continue;

            vector<string> row;
            if (result.columns.size() == 3) {
                row = {fields[0],
                      fields.size() > 1 ? fields[1] : "",
                      fields.size() > 2 ? fields[2] : ""};
            } else if (result.columns[0] == "Name") {
                row = {fields.size() > 1 ? fields[1] : ""};
            } else if (result.columns[0] == "ID") {
                row = {fields[0]};
            } else if (result.columns[0] == "Address") {
                row = {fields.size() > 2 ? fields[2] : ""};
            }
            result.rows.push_back(row);
        }
    } else {
        // Identify what type of WHERE clause we have
        string whereType = identifyWhereClauseType(whereClause);

        if (whereType == "doctor_id") {
            // Use primary index (Doctor ID)
            string doctorID = extractValueFromWhereClause(whereClause, "doctor id");
            if (doctorID.empty()) {
                doctorID = extractValueFromWhereClause(whereClause, "id");
            }

            if (!doctorID.empty()) {
                try {
                    int id = stoi(doctorID);
                    short offset = searchForDocID(id);

                    if (offset != -1) {
                        // Read doctor record
                        fstream f(DOCTOR_FILE, ios::in | ios::binary);
                        f.seekg(offset);
                        int rec_len;
                        f.read(reinterpret_cast<char*>(&rec_len), sizeof(rec_len));
                        vector<char> buf(rec_len);
                        f.read(buf.data(), rec_len);
                        f.close();

                        string body(buf.begin(), buf.end());
                        vector<string> fields = splitFields(body);

                        if (result.columns.size() == 3) {
                            result.rows.push_back({fields[0],
                                                  fields.size() > 1 ? fields[1] : "",
                                                  fields.size() > 2 ? fields[2] : ""});
                        } else if (result.columns[0] == "Name") {
                            result.rows.push_back({fields.size() > 1 ? fields[1] : ""});
                        } else if (result.columns[0] == "ID") {
                            result.rows.push_back({fields[0]});
                        } else if (result.columns[0] == "Address") {
                            result.rows.push_back({fields.size() > 2 ? fields[2] : ""});
                        }
                    }
                } catch (...) {
                    // Invalid ID format
                }
            }
        } else if (whereType == "doctor_name") {
            // Use secondary index (Doctor Name)
            string doctorName = extractValueFromWhereClause(whereClause, "doctor name");
            if (doctorName.empty()) {
                doctorName = extractValueFromWhereClause(whereClause, "name");
            }

            if (!doctorName.empty()) {
                // Search using secondary index
                vector<string> doctorIDs = searchDoctorByName(doctorName);

                if (!doctorIDs.empty()) {
                    // Load index to get full records
                    unordered_map<string, RecIndex> idx;
                    loadPrimaryIndex(DOCTOR_INDEX_FILE, DOCTOR_FILE, idx);

                    for (const string& id : doctorIDs) {
                        auto it = idx.find(id);
                        if (it != idx.end()) {
                            vector<string> fields = splitFields(it->second.body);

                            if (result.columns.size() == 3) {
                                result.rows.push_back({fields[0],
                                                      fields.size() > 1 ? fields[1] : "",
                                                      fields.size() > 2 ? fields[2] : ""});
                            } else if (result.columns[0] == "Name") {
                                result.rows.push_back({fields.size() > 1 ? fields[1] : ""});
                            } else if (result.columns[0] == "ID") {
                                result.rows.push_back({fields[0]});
                            } else if (result.columns[0] == "Address") {
                                result.rows.push_back({fields.size() > 2 ? fields[2] : ""});
                            }
                        }
                    }
                }
            }
        } else {
            cout << "Unsupported WHERE clause for Doctors\n";
        }
    }

    return result;
}

QueryResult parseAndExecuteQuery(const string& query) {
    QueryResult result;

    if (query.empty()) {
        cout << "Empty query!\n";
        return result;
    }

    // Convert to lowercase for case-insensitive parsing
    string lowerQuery = toLower(query);

    // Basic query parsing
    if (lowerQuery.find("select") == string::npos) {
        cout << "Invalid query: Must start with SELECT\n";
        return result;
    }

    // Parse SELECT clause
    size_t selectPos = 6; // after "select "
    size_t fromPos = lowerQuery.find("from");
    if (fromPos == string::npos) {
        cout << "Invalid query: Missing FROM clause\n";
        return result;
    }

    string selectClause = query.substr(selectPos, fromPos - selectPos);
    trim(selectClause);

    // Parse FROM clause
    size_t wherePos = lowerQuery.find("where");
    string fromClause;
    if (wherePos == string::npos) {
        fromClause = lowerQuery.substr(fromPos + 4);
        trim(fromClause);
    } else {
        fromClause = lowerQuery.substr(fromPos + 4, wherePos - fromPos - 4);
        trim(fromClause);
    }

    // Parse WHERE clause
    string whereClause;
    if (wherePos != string::npos) {
        whereClause = query.substr(wherePos + 5);
        trim(whereClause);
        // Remove trailing semicolon if present
        if (!whereClause.empty() && whereClause.back() == ';') {
            whereClause.pop_back();
        }
        trim(whereClause);
    }

    // Remove trailing semicolon from FROM clause if present
    if (!fromClause.empty() && fromClause.back() == ';') {
        fromClause.pop_back();
        trim(fromClause);
    }

    // Determine table
    if (fromClause == "doctors") {
        return executeDoctorQuery(selectClause, whereClause);
    } else if (fromClause == "appointments") {
        return executeAppointmentQuery(selectClause, whereClause);
    } else {
        cout << "Unknown table: " << fromClause << "\n";
        return result;
    }
}

// Function to display query results
void displayQueryResult(const QueryResult& result) {
    if (result.columns.empty() || result.rows.empty()) {
        cout << "No results found.\n";
        return;
    }

    // Display column headers
    for (size_t i = 0; i < result.columns.size(); ++i) {
        cout << result.columns[i];
        if (i < result.columns.size() - 1) cout << " | ";
    }
    cout << "\n";

    // Display separator
    cout << string(50, '-') << "\n";

    // Display rows
    for (const auto& row : result.rows) {
        for (size_t i = 0; i < row.size(); ++i) {
            cout << row[i];
            if (i < row.size() - 1) cout << " | ";
        }
        cout << "\n";
    }
}

// -------------------- Menu --------------------
void menu() {
    // Ensure all files exist (data files get avail-head; index & LL files are empty files)
    ensureDataFile(DOCTOR_FILE);
    ensureDataFile(APPOINTMENT_FILE);

    ensureIndexFile(DOCTOR_INDEX_FILE);
    ensureIndexFile(APPOINTMENT_INDEX_FILE);
    ensureIndexFile(DOCTOR_SECONDRY_INDEX_FILE);
    ensureIndexFile(APPOINTMENT_SECONDRY_INDEX_FILE);
    ensureIndexFile(LL_DOCTOR_FILE);
    ensureIndexFile(LL_APPOINTMENT_FILE);

    // Load counts
    DocCountID = loadCountID(DOCTOR_INDEX_FILE);
    appointmentCountID = loadCountID(APPOINTMENT_INDEX_FILE);

    int choice = 0;
    do {
        cout << "\n=== Healthcare Management ===\n";
        cout << "1. Add Doctor\n2. Delete Doctor\n3. Update Doctor Name\n4. Print Doctor\n5. List All Doctors\n";
        cout << "6. Add Appointment\n7. Delete Appointment\n8. Update Appointment Date\n9. Print Appointment\n10. List All Appointments\n";
        cout << "11. Execute Query\n12. Exit\n";
        cout << "Choice: ";

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            cout << "Invalid input. Please enter a number.\n";
            continue;
        }

        switch(choice) {
            case 1: addDoctor(); break;
            case 2: deleteDoctor(); break;
            case 3: updateDoctorName(); break;
            case 4: printDoctorInfo(); break;
            case 5: listAllDoctors(); break;
            case 6: addAppointment(); break;
            case 7: deleteAppointment(); break;
            case 8: updateAppointmentDate(); break;
            case 9: printAppointmentInfo(); break;
            case 10: listAllAppointments(); break;
            case 11: {
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                string query;
                cout << "Enter query: ";
                getline(cin, query);
                if (!query.empty()) {
                    QueryResult result = parseAndExecuteQuery(query);
                    displayQueryResult(result);
                } else {
                    cout << "Empty query!\n";
                }
                break;
            }
            case 12: cout << "Exiting...\n"; break;
            default: cout << "Invalid choice. Please enter 1-12.\n";
        }

        // Clear any leftover input
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

    } while(choice != 12);
}

int main() {
    cout << "Healthcare Management System\n";
    menu();
    return 0;
}
