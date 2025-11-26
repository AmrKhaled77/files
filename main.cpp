#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>

using namespace std;

// -------------------- Files --------------------
const string DOCTOR_FILE = "Doctors.dat";
const string APPOINTMENT_FILE = "Appointments.dat";

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

// -------------------- Index --------------------
struct RecIndex {
    long long recOffset;
    int recLen;
    string id;
    string body;
};

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
    if (slot != -1) writeActiveBodyAt(f, slot, body), cout << "Doctor added (reused deleted slot)\n";
    else appendRecord(f, body), cout << "Doctor appended at file end\n";
    f.close();
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

// -------------------- Appointment operations --------------------
void addAppointment() {
    unordered_map<string, RecIndex> docIdx;
    loadIndex(DOCTOR_FILE, docIdx);
    unordered_map<string, RecIndex> apIdx;
    loadIndex(APPOINTMENT_FILE, apIdx);

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
    if (slot != -1) writeActiveBodyAt(f, slot, body), cout << "Appointment added (reused deleted slot)\n";
    else appendRecord(f, body), cout << "Appointment appended at file end\n";
    f.close();
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

// -------------------- Menu --------------------
void menu() {
    ensureFile(DOCTOR_FILE);
    ensureFile(APPOINTMENT_FILE);
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
