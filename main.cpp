#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>
#include <algorithm>

using namespace std;

short DocCountID = 0;
short appointmentCountID = 0;

// -------------------- Files --------------------
const string DOCTOR_FILE = "Doctors.dat";
const string APPOINTMENT_FILE = "Appointments.dat";
const string DOCTOR_INDEX_FILE = "DoctorIndex.ind";
const string APPOINTMENT_INDEX_FILE = "AppointmentIndex.ind";
const string DOCTOR_NAME_INDEX_FILE = "DoctorNameIndex.ind";
const string DOCTOR_ID_INDEX_FILE = "DoctoridIndex.ind";
static const size_t INDEX_RECORD_SIZE = sizeof(int) + sizeof(long long);

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

void ensureFile(const string &fname) {
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

long long fileEndOffset(fstream &f) {
    f.seekg(0, ios::end);
    return static_cast<long long>(f.tellg());
}

// Split fields by '|'
vector<string> splitFields(const string &s) {
    vector<string> out;
    string cur;
    for (char c : s) {
        if (c == '|') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

short loadCountID(const string &indexFile) {

    ifstream f(indexFile, ios::binary);
    if (!f.is_open()) return 0;
    f.seekg(0, ios::end);
    long long bytes = f.tellg();
    f.close();
    return static_cast<short>(bytes / INDEX_RECORD_SIZE);
}

int idStringToInt(const string &id) {
    int x = 0;
    for (char c : id) {
        if (c >= '0' && c <= '9') {
            x = x * 10 + (c - '0');
        } else {
            // If id contains non-digit chars, use std::hash fallback
            return static_cast<int>(std::hash<string>()(id));
        }
    }
    return x;
}

// --------------------Primary Index --------------------
struct RecIndex {
    long long recOffset;
    int recLen;
    string id;
    string body;
};
void insertInPrimaryIndex(const string &keyIdStr, long long recOffset, const string& indexFileName, short &CountID) {
    ensureFile(indexFileName);
    fstream f(indexFileName, ios::in | ios::out | ios::binary);

    int key = idStringToInt(keyIdStr);
    if (CountID == 0) {
        f.seekp(0, ios::end);
        f.write(reinterpret_cast<char*>(&key), sizeof(key));
        f.write(reinterpret_cast<char*>(&recOffset), sizeof(recOffset));
        f.flush();
        f.close();
        CountID++;
        return;
    }
    short lo = 0, hi = CountID - 1;
    short pos = CountID;
    while (lo <= hi) {
        short mid = (lo + hi) / 2;
        f.seekg(mid * INDEX_RECORD_SIZE, ios::beg);
        int midKey;
        f.read(reinterpret_cast<char*>(&midKey), sizeof(midKey));
        if (!f) break;
        if (midKey == key) {
            // duplicate: replace offset (update) and return
            f.read(reinterpret_cast<char*>(&recOffset), sizeof(recOffset)); // move stream
            f.seekp(mid * INDEX_RECORD_SIZE + sizeof(int), ios::beg);
            f.write(reinterpret_cast<char*>(&recOffset), sizeof(recOffset));
            f.flush();
            f.close();
            return;
        } else if (midKey < key) {
            lo = mid + 1;
        } else {
            if (mid == 0) { pos = 0; break; }
            hi = mid - 1;
            pos = mid;
        }
    }
    // pos is insertion index (0..CountID)
    if (pos == CountID) {
        // append
        f.seekp(0, ios::end);
        f.write(reinterpret_cast<char*>(&key), sizeof(key));
        f.write(reinterpret_cast<char*>(&recOffset), sizeof(recOffset));
        f.flush();
        CountID++;
        f.close();
        return;
    }
    vector<char> tail;
    f.seekg(pos * INDEX_RECORD_SIZE, ios::beg);
    {
        f.seekg(0, ios::end);
        long long fileEnd = f.tellg();
        long long tailBytes = fileEnd - (pos * INDEX_RECORD_SIZE);
        tail.resize(static_cast<size_t>(tailBytes));
        f.seekg(pos * INDEX_RECORD_SIZE, ios::beg);
        if (tailBytes > 0) f.read(tail.data(), tailBytes);
    }
    // write back: new record, then old tail
    f.seekp(pos * INDEX_RECORD_SIZE, ios::beg);
    f.write(reinterpret_cast<char*>(&key), sizeof(key));
    f.write(reinterpret_cast<char*>(&recOffset), sizeof(recOffset));
    if (!tail.empty()) f.write(tail.data(), tail.size());
    f.flush();
    f.close();
    CountID++;
}

long long BinarySearchId(int ID1, const string& indexName, short CountID) {
    ensureFile(indexName);
    fstream f(indexName, ios::in | ios::binary);
    if (!f.is_open()) return -1;
    short first = 0;
    short last = CountID - 1;
    while (first <= last) {
        short mid = (first + last) / 2;
        f.seekg(mid * INDEX_RECORD_SIZE, ios::beg);
        int midKey;
        long long midOffset;
        f.read(reinterpret_cast<char*>(&midKey), sizeof(midKey));
        f.read(reinterpret_cast<char*>(&midOffset), sizeof(midOffset));
        if (!f) break;
        if (midKey == ID1) {
            f.close();
            return midOffset;
        } else if (midKey > ID1) {
            if (mid == 0) break;
            last = mid - 1;
        } else {
            first = mid + 1;
        }
    }
    f.close();
    return -1;
}

void Deleteprimary(int idInt, const string& indexName, short &CountID) {
    ensureFile(indexName);
    fstream f(indexName, ios::in | ios::out | ios::binary);
    if (!f.is_open()) return;
    short first = 0;
    short last = CountID - 1;
    short foundAt = -1;
    while (first <= last) {
        short mid = (first + last) / 2;
        f.seekg(mid * INDEX_RECORD_SIZE, ios::beg);
        int midKey;
        f.read(reinterpret_cast<char*>(&midKey), sizeof(midKey));
        if (!f) break;
        if (midKey == idInt) { foundAt = mid; break; }
        else if (midKey > idInt) {
            if (mid == 0) break;
            last = mid - 1;
        } else first = mid + 1;
    }
    if (foundAt == -1) { f.close(); return; }

    // If last entry, just truncate file
    if (foundAt == CountID - 1) {
        f.close();
        // truncate file by rewriting up to (CountID-1)*INDEX_RECORD_SIZE
        // Simple portable approach: read everything except last, rewrite file
        ifstream in(indexName, ios::binary);
        vector<char> buff((CountID - 1) * INDEX_RECORD_SIZE);
        in.read(buff.data(), buff.size());
        in.close();
        ofstream out(indexName, ios::binary | ios::trunc);
        if (!buff.empty()) out.write(buff.data(), buff.size());
        out.close();
        CountID--;
        return;
    }

    // shift bytes after foundAt left by one record
    f.seekg((foundAt + 1) * INDEX_RECORD_SIZE, ios::beg);
    vector<char> tail;
    f.seekg(0, ios::end);
    long long fileEnd = f.tellg();
    long long tailBytes = fileEnd - ((foundAt + 1) * INDEX_RECORD_SIZE);
    tail.resize(static_cast<size_t>(tailBytes));
    f.seekg((foundAt + 1) * INDEX_RECORD_SIZE, ios::beg);
    if (tailBytes > 0) f.read(tail.data(), tailBytes);

    f.seekp(foundAt * INDEX_RECORD_SIZE, ios::beg);
    if (!tail.empty()) f.write(tail.data(), tail.size());
    f.flush();
    f.close();

    ifstream in2(indexName, ios::binary);
    vector<char> buff((CountID - 1) * INDEX_RECORD_SIZE);
    in2.read(buff.data(), buff.size());
    in2.close();
    ofstream out2(indexName, ios::binary | ios::trunc);
    if (!buff.empty()) out2.write(buff.data(), buff.size());
    out2.close();

    CountID--;
}

void loadIndex(const string &fname, unordered_map<string, RecIndex> &indexMap) {
    indexMap.clear();
    ensureFile(fname);
    fstream f(fname, ios::in | ios::out | ios::binary);
    long long avail = -1;
    f.read(reinterpret_cast<char*>(&avail), sizeof(avail));
    long long pos = sizeof(avail);
    while (true) {
        f.seekg(pos);
        int rec_len;
        if (!f.read(reinterpret_cast<char*>(&rec_len), sizeof(rec_len))) break;
        if (rec_len <= 0) break;
        long long bodyPos = pos + sizeof(rec_len);
        vector<char> buf(rec_len);
        f.seekg(bodyPos);
        if (!f.read(buf.data(), rec_len)) break;
        if (buf.size() == 0) break;
        if (buf[0] != '*') {
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
        pos = bodyPos + rec_len;
    }
    f.close();
}
//for testing
void printPrimaryIndex(short CountID ,  const string& indexName) {
    fstream prim(indexName, ios::in | ios::binary);

    if (!prim.is_open()) {
        cout << "Cannot open index file!\n";
        return;
    }

    cout << "\n=== Primary Index Content (" <<"doctor" << ") ===\n";
    cout << "Total Entries: " << CountID << "\n\n";

    int id;
    short offset;

    for (int i = 0; i < CountID; i++) {
        prim.seekg(i * 6, ios::beg);

        prim.read(reinterpret_cast<char*>(&id), sizeof(id));
        prim.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        if (!prim.good()) break;

        cout << "Entry " << i
             << " | ID = " << id
             << " | Offset = " << offset
             << "\n";
    }

    prim.close();
    cout << "===========================================\n";
}
//-------------------Secondry index--------------------
struct DoctorNameIndexEntry {
    string name;   // Doctor name
    int doctorID;  // Primary key (since names may repeat)
};
struct AppointmentDoctorIndexEntry {
    int doctorID;  // Which doctor this appointment belongs to
    int offset;    // Offset of appointment record in Appointments.dat
};


//-------------------Query parser--------------------

vector<string> readDelimitedRecord(const string &fileName, long long offset) {
    fstream f(fileName, ios::in | ios::binary);
    if (!f) return {};

    f.seekg(offset, ios::beg);

    int size;
    f.read((char*)&size, sizeof(size));
    if (f.fail() || size <= 0) return {};

    string raw(size, '\0');
    f.read(&raw[0], size);

    // Split by '|'
    vector<string> fields;
    string temp = "";
    for (char c : raw) {
        if (c == '|') {
            fields.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }

    return fields;
}
vector<string> getDoctorRecord(long long offset) {
    return readDelimitedRecord(DOCTOR_FILE, offset);
}
vector<string> getAppointmentRecord(long long offset) {
    return readDelimitedRecord(APPOINTMENT_FILE, offset);
}

struct ParsedQuery {
    string columns;
    string table;
    string whereColumn;
    string whereValue;
};


static string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    size_t end = s.find_last_not_of(" \t\n\r");
    if (start == string::npos) return "";
    return s.substr(start, end - start + 1);
}
string join(const vector<string>& v, const string& sep) {
    string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i) out += sep;
        out += v[i];
    }
    return out;
}

string extractColumns(const vector<string>& fields, const string& table, const string& columns) {
    // SELECT *
    if (columns == "*" || columns == "all")
        return join(fields, ", ");

    // Split selected columns
    vector<string> cols;
    string tmp;
    for (char c : columns) {
        if (c == ',') {
            cols.push_back(trim(tmp));
            tmp.clear();
        } else tmp += c;
    }
    if (!tmp.empty()) cols.push_back(trim(tmp));

    // Doctor: id, name, address
    if (table == "doctor") {
        vector<pair<string,int>> map = {
            {"doctorid", 0}, {"name", 1}, {"address", 2}
        };

        string out = "";
        for (auto& c : cols) {
            for (auto& p : map) {
                if (trim(c) == p.first && p.second < fields.size()) {
                    if (!out.empty()) out += ", ";
                    out += fields[p.second];
                }
            }
        }
        return out;
    }

    // Appointment: id, date, doctorId
    if (table == "appointment") {
        vector<pair<string,int>> map = {
            {"appointmentid", 0}, {"date", 1}, {"doctorid", 2}
        };

        string out = "";
        for (auto& c : cols) {
            for (auto& p : map) {
                if (trim(c) == p.first && p.second < fields.size()) {
                    if (!out.empty()) out += ", ";
                    out += fields[p.second];
                }
            }
        }
        return out;
    }

    return "";
}

ParsedQuery parseSelect(const string &input) {
    string Query = input;
    string lower = Query;
    transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    ParsedQuery r;

    size_t selectPos = lower.find("select ");
    size_t fromPos   = lower.find(" from ");
    size_t wherePos  = lower.find(" where ");

    r.columns = trim(Query.substr(selectPos + 7, fromPos - (selectPos + 7)));

    if (wherePos == string::npos) {
        r.table = trim(Query.substr(fromPos + 6));
        return r;
    }

    r.table = trim(Query.substr(fromPos + 6, wherePos - (fromPos + 6)));

    string wherePart = Query.substr(wherePos + 7);
    size_t eqPos = wherePart.find('=');

    r.whereColumn = trim(wherePart.substr(0, eqPos));
    r.whereValue  = trim(wherePart.substr(eqPos + 1));

    return r;
}

string applyQuery(ParsedQuery query) {
    if (query.columns.empty() || query.whereColumn.empty())
        return "";

    // ---------------- DOCTOR: WHERE doctorid ----------------
    if (query.table == "doctor" && query.whereColumn == "doctorid") {
        long long offset = BinarySearchId(stoi(query.whereValue), DOCTOR_INDEX_FILE, DocCountID);
        if (offset == -1) return "";
        vector<string> record = getDoctorRecord(offset);
        return extractColumns(record, "doctor", query.columns);
    }

    // ---------------- APPOINTMENT: WHERE appointmentid ----------------
    else if (query.table == "appointment" && query.whereColumn == "appointmentid") {
        long long offset = BinarySearchId(stoi(query.whereValue), APPOINTMENT_INDEX_FILE, appointmentCountID);
        if (offset == -1) return "";
        vector<string> record = getAppointmentRecord(offset);
        return extractColumns(record, "appointment", query.columns);
    }

    // ---------------- DOCTOR: WHERE doctor name (full scan needed) ----------------
    else if (query.table == "doctor" && query.whereColumn == "doctorname") {
        // next step → I can generate full-scan search function if you want
    }

    // ---------------- APPOINTMENT: WHERE doctorId (needs inverted index or full scan) ----------------
    else if (query.table == "appointment" && query.whereColumn == "doctorid") {
        // next step → I can generate function to fetch all appointments by doctor
    }

    return "";
}


// -------------------- AVAIL list --------------------
long long readAvailHead(const string &fname) {
    ensureFile(fname);
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
    f.write(reinterpret_cast<char*>(&nextPtr), sizeof(nextPtr));
    int pad = rec_len - (1 + sizeof(nextPtr));
    if (pad > 0) {
        string zeros(pad, ' ');
        f.write(zeros.data(), pad);
    }
    f.flush();
}

void writeActiveBodyAt(fstream &f, long long recOffset, const string &body) {
    int rec_len = readIntAt(f, recOffset);
    if ((int)body.size() > rec_len) throw runtime_error("body too big for slot");
    long long bodyPos = recOffset + sizeof(rec_len);
    f.seekp(bodyPos);
    f.write(body.data(), body.size());
    int pad = rec_len - (int)body.size();
    if (pad > 0) {
        string spaces(pad, ' ');
        f.write(spaces.data(), pad);
    }
    f.flush();
}

long long appendRecord(fstream &f, const string &body) {
    f.seekp(0, ios::end);
    long long recOffset = static_cast<long long>(f.tellp());
    int rec_len = static_cast<int>(body.size());
    f.write(reinterpret_cast<char*>(&rec_len), sizeof(rec_len));
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
            if (prev == -1) writeLongAt(f, 0, nextPtr);
            else {
                long long prevBodyPos = prev + sizeof(int);
                f.seekp(prevBodyPos + 1);
                f.write(reinterpret_cast<char*>(&nextPtr), sizeof(nextPtr));
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
    loadIndex(DOCTOR_FILE, idx);
    long long recOffset;

    string id, name, address;
    cout << "Enter Doctor ID: "; cin >> id;
    if (idx.find(id) != idx.end()) { cout << "Doctor already exists!\n"; return; }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    cout << "Enter Name: "; getline(cin, name);
    cout << "Enter Address: "; getline(cin, address);

    string body = id + "|" + name + "|" + address;
    int needed = static_cast<int>(body.size());

    ensureFile(DOCTOR_FILE);
    long long slot = findFitInAvailAndRemove(DOCTOR_FILE, needed);
    fstream f(DOCTOR_FILE, ios::in | ios::out | ios::binary);


    if (slot != -1) {
        writeActiveBodyAt(f, slot, body);
        recOffset = slot;
        cout << "Doctor added (reused deleted slot)\n";
    }
    else {
        recOffset = appendRecord(f, body);
        cout << "Doctor appended at file end\n";
    }
    f.close();

    /* ---- UPDATE PRIMARY INDEX ---- */
    insertInPrimaryIndex(id, recOffset, DOCTOR_INDEX_FILE,DocCountID);



}

void deleteDoctor() {
    unordered_map<string, RecIndex> idx;
    loadIndex(DOCTOR_FILE, idx);
    string id; cout << "Enter Doctor ID to delete: "; cin >> id;
    auto it = idx.find(id);
    if (it == idx.end()) { cout << "Doctor not found!\n"; return; }
    fstream f(DOCTOR_FILE, ios::in | ios::out | ios::binary);
    long long head = readLongAt(f, 0);
    writeDeletedAt(f, it->second.recOffset, head);
    writeLongAt(f, 0, it->second.recOffset);
    f.close();
    cout << "Doctor logically deleted\n";

    /* ---- DELETE FROM PRIMARY INDEX ---- */
    int num = stoi(id);
    Deleteprimary(num, DOCTOR_INDEX_FILE,DocCountID);

}

void updateDoctorName() {
    unordered_map<string, RecIndex> idx;
    loadIndex(DOCTOR_FILE, idx);
    string id; cout << "Enter Doctor ID: "; cin >> id;
    auto it = idx.find(id);
    if (it == idx.end()) { cout << "Doctor not found!\n"; return; }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string newName; cout << "Enter new Name: "; getline(cin, newName);
    vector<string> fields = splitFields(it->second.body);
    string newBody = id + "|" + newName + "|" + (fields.size()>2?fields[2]:"");
    if ((int)newBody.size() > it->second.recLen) { cout << "New data too long\n"; return; }
    fstream f(DOCTOR_FILE, ios::in | ios::out | ios::binary);
    writeActiveBodyAt(f, it->second.recOffset, newBody);
    f.close();
    cout << "Doctor name updated\n";
}

void printDoctorInfo() {
    unordered_map<string, RecIndex> idx;
    loadIndex(DOCTOR_FILE, idx);
    string id; cout << "Enter Doctor ID: "; cin >> id;
    auto it = idx.find(id);
    if (it == idx.end()) { cout << "Doctor not found!\n"; return; }
    vector<string> fields = splitFields(it->second.body);
    cout << "ID: " << fields[0] << "\nName: " << (fields.size()>1?fields[1]:"") << "\nAddress: " << (fields.size()>2?fields[2]:"") << "\n";
}

void listAllDoctors() {
    unordered_map<string, RecIndex> idx;
    loadIndex(DOCTOR_FILE, idx);
    cout << "=== Doctors ===\n";
    for (auto &p: idx) {
        vector<string> f = splitFields(p.second.body);
        cout << p.second.id << " | " << (f.size()>1?f[1]:"") << " | " << (f.size()>2?f[2]:"") << "\n";
    }
}

short searchForDocID(int id) {
   return  BinarySearchId(id ,DOCTOR_INDEX_FILE, DocCountID);
}

// -------------------- Appointment operations --------------------
void addAppointment() {
    unordered_map<string, RecIndex> docIdx;
    loadIndex(DOCTOR_FILE, docIdx);
    unordered_map<string, RecIndex> apIdx;
    loadIndex(APPOINTMENT_FILE, apIdx);
    long long recOffset;

    string appID; cout << "Enter Appointment ID: "; cin >> appID;
    if (apIdx.find(appID) != apIdx.end()) { cout << "Appointment exists!\n"; return; }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string date, doctorID;
    cout << "Enter Appointment Date: "; getline(cin, date);
    cout << "Enter Doctor ID: "; getline(cin, doctorID);
    if (docIdx.find(doctorID) == docIdx.end()) { cout << "Doctor does not exist!\n"; return; }

    string body = appID + "|" + date + "|" + doctorID;
    int needed = (int)body.size();
    ensureFile(APPOINTMENT_FILE);
    long long slot = findFitInAvailAndRemove(APPOINTMENT_FILE, needed);
    fstream f(APPOINTMENT_FILE, ios::in | ios::out | ios::binary);
    if (slot != -1) {
        writeActiveBodyAt(f, slot, body);
        cout << "Appointment added (reused deleted slot)\n";
        recOffset=slot;
    }
    else appendRecord(f, body), cout << "Appointment appended at file end\n";
    f.close();
    /* ---- UPDATE PRIMARY INDEX FOR APPOINTMENT ---- */
    insertInPrimaryIndex(appID,recOffset,APPOINTMENT_INDEX_FILE,appointmentCountID);

}

void deleteAppointment() {
    unordered_map<string, RecIndex> idx;
    loadIndex(APPOINTMENT_FILE, idx);
    string id; cout << "Enter Appointment ID: "; cin >> id;
    auto it = idx.find(id);
    if (it == idx.end()) { cout << "Appointment not found!\n"; return; }
    fstream f(APPOINTMENT_FILE, ios::in | ios::out | ios::binary);
    long long head = readLongAt(f, 0);
    writeDeletedAt(f, it->second.recOffset, head);
    writeLongAt(f, 0, it->second.recOffset);
    f.close();
    /* ---- DELETE FROM PRIMARY INDEX FOR APPOINTMENT ---- */
    int num = stoi(id);
    Deleteprimary(num, APPOINTMENT_INDEX_FILE,appointmentCountID);
    cout << "Appointment logically deleted\n";

}

void updateAppointmentDate() {
    unordered_map<string, RecIndex> idx;
    loadIndex(APPOINTMENT_FILE, idx);
    string id; cout << "Enter Appointment ID: "; cin >> id;
    auto it = idx.find(id);
    if (it == idx.end()) { cout << "Appointment not found!\n"; return; }
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    string newDate; cout << "Enter new Date: "; getline(cin, newDate);
    vector<string> flds = splitFields(it->second.body);
    string newBody = id + "|" + newDate + "|" + (flds.size()>2?flds[2]:"");
    if ((int)newBody.size() > it->second.recLen) { cout << "New data too long\n"; return; }
    fstream f(APPOINTMENT_FILE, ios::in | ios::out | ios::binary);
    writeActiveBodyAt(f, it->second.recOffset, newBody);
    f.close();
    cout << "Appointment updated\n";
}

void printAppointmentInfo() {
    unordered_map<string, RecIndex> idx;
    loadIndex(APPOINTMENT_FILE, idx);
    string id; cout << "Enter Appointment ID: "; cin >> id;
    auto it = idx.find(id);
    if (it == idx.end()) { cout << "Appointment not found!\n"; return; }
    vector<string> f = splitFields(it->second.body);
    cout << "ID: " << f[0] << "\nDate: " << (f.size()>1?f[1]:"") << "\nDoctorID: " << (f.size()>2?f[2]:"") << "\n";
}

void listAllAppointments() {
    unordered_map<string, RecIndex> idx;
    loadIndex(APPOINTMENT_FILE, idx);
    cout << "=== Appointments ===\n";
    for (auto &p: idx) {
        vector<string> f = splitFields(p.second.body);
        cout << p.second.id << " | " << (f.size()>1?f[1]:"") << " | " << (f.size()>2?f[2]:"") << "\n";
    }
}

short searchForAppointID(int id) {
    return  BinarySearchId(id ,APPOINTMENT_INDEX_FILE, appointmentCountID);
}


// -------------------- Menu --------------------
void menu() {
    ensureFile(DOCTOR_FILE);
    ensureFile(APPOINTMENT_FILE);
    ensureFile(DOCTOR_INDEX_FILE);
    DocCountID = loadCountID(DOCTOR_INDEX_FILE);
    appointmentCountID = loadCountID(APPOINTMENT_INDEX_FILE);


    int choice = 0;
    do {
        cout << "\n=== Healthcare Management ===\n";
        cout << "1. Add Doctor\n2. Delete Doctor\n3. Update Doctor Name\n4. Print Doctor\n5. List All Doctors\n";
        cout << "6. Add Appointment\n7. Delete Appointment\n8. Update Appointment Date\n9. Print Appointment\n10. List All Appointments\n11. Exit\n";
        cout << "Choice: "; cin >> choice; 
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
            case 11: cout << "Exiting...\n"; break;
            default: cout << "Invalid choice\n";
        }
    } while(choice != 11);
}

int main() {
    cout << "Healthcare Management System (Doctors + Appointments)\n";
    menu();
    return 0;
}
