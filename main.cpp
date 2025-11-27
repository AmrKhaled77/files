#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <limits>

using namespace std;

short DocCountID = 0;
short appointmentCountID = 0;

// -------------------- Files --------------------
const string DOCTOR_FILE = "Doctors.dat";
const string APPOINTMENT_FILE = "Appointments.dat";
const string DOCTOR_INDEX_FILE = "DoctorIndex.ind";
const string APPOINTMENT_INDEX_FILE = "AppointmentIndex.ind";

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

int loadCountID(const string &indexFile) {
    fstream f(indexFile, ios::binary | ios::in);
    if (!f.is_open()) return 0;

    f.seekg(0, ios::end);
    long long bytes = f.tellg();
    f.close();

    return bytes / 6;   // each index record = 4 bytes key + 2 bytes offset
}


// -------------------- Index --------------------
struct RecIndex {
    long long recOffset;
    int recLen;
    string id;
    string body;
};

void insertInPrimaryIndex(char id[], short offset ,string indexFileName ,short CountID) // insert sorted in the primary index
{
	fstream prim(indexFileName, ios::out | ios::binary | ios::in);
	int x = 0;
	for (int i = 0; id[i] != '\0'; i++)
	{
		x *= 10;
		x += (id[i] - '0');
	}
	int tmp = 0;
	short of = 0;
	bool hi = false;
	if (CountID == 0)
	{
		prim.write((char*)&x, sizeof(int));
		prim.write((char*)&offset, sizeof(short));
		CountID++;
		return;
	}
	prim.read((char*)&tmp, sizeof(tmp));
	while (prim.good())
	{
		if (tmp > x)
		{
			hi = true;
			prim.seekg(-4, ios::cur);
			of = prim.tellg();
			break;
		}

		prim.seekg(2, ios::cur);
		prim.read((char*)&tmp, sizeof(tmp));

	}
	prim.close();
	prim.open(indexFileName, ios::out | ios::binary | ios::in);

	if (!hi){ ///in the last position
		prim.seekg(CountID * 6, ios::beg);
		prim.write((char*)&x, sizeof(int));
		prim.write((char*)&offset, sizeof(short));
		CountID++;
	}
	else{
		prim.seekg((CountID - 1) * 6, ios::beg);
		int numend;
		short ofend;
		prim.read((char*)& numend, sizeof(numend));
		prim.read((char*)& ofend, sizeof(ofend));
		prim.seekg(of, ios::beg);

		while (prim.good())
		{
			int tmpnum; short tmpof;
			int tmpnum1; short tmpof1;
			prim.read((char*)& tmpnum, sizeof(tmpnum));
			prim.read((char*)& tmpof, sizeof(tmpof));
			prim.read((char*)& tmpnum1, sizeof(tmpnum1));
			prim.read((char*)& tmpof1, sizeof(tmpof1));
			prim.seekg(-6, ios::cur);
			prim.write((char*)& tmpnum, sizeof(tmpnum));
			prim.write((char*)& tmpof, sizeof(tmpof));

		}
		prim.close();
		prim.open(indexFileName, ios::out | ios::in | ios::binary);
		prim.seekg(0, ios::end);

		prim.write((char*)& numend, sizeof(numend));
		prim.write((char*)& ofend, sizeof(ofend));
		prim.seekg(of, ios::beg);
		prim.write((char*)& x, sizeof(x));
		prim.write((char*)& offset, sizeof(of));
		CountID++;
	}
	prim.close();

}
short BinarySearchId(int ID1,string indexName ,short CountID) // binary search in the primary index
{
    fstream prim(indexName, ios::in | ios::binary | ios::out);
    short first = 0;
    short last = CountID - 1;
    short mid;
    bool found = false;
    int temp;
    while (first <= last && !found)
    {
        mid = (first + last) / 2;
        prim.seekg(mid * 6, ios::beg);
        prim.read((char*)&temp, sizeof(temp));

        if (temp == ID1)
            found = true;
        else if (temp >  ID1)
            last = mid - 1;
        else
            first = mid + 1;
    }
    if (found)
    {
        short of;
        prim.seekg((mid * 6) + 4, ios::beg);
        prim.read((char*)&of, sizeof(of));
        prim.close();
        return of;
    }
    else
    {
        short nega=-1;
        prim.close();
        return nega;

    }

}
void Deleteprimary(int ID1 ,string indexName ,short CountID) // delete from primary index
{

    fstream prim(indexName, ios::in | ios::binary | ios::out);
    short first = 0;
    short last = CountID - 1;
    short mid;
    bool found = false;
    int temp;
    while (first <= last && !found)
    {
        mid = (first + last) / 2;
        prim.seekg(mid * 6, ios::beg);
        prim.read((char*)&temp, sizeof(temp));

        if (temp == ID1)
            found = true;
        else if (temp >  ID1)
            last = mid - 1;
        else
            first = mid + 1;
    }
    if (found)
    {
        prim.seekg((mid + 1) * 6, ios::beg);

        while (prim.good()) /// start to shift
        {
            int tmpnum; short tmpof;
            prim.read((char*)& tmpnum, sizeof(tmpnum));
            prim.read((char*)& tmpof, sizeof(tmpof));

            prim.seekg(-12, ios::cur);
            prim.write((char*)& tmpnum, sizeof(tmpnum));
            prim.write((char*)& tmpof, sizeof(tmpof));
            prim.seekg(6, ios::cur);

        }
        prim.close();
        prim.open(indexName, ios::out | ios::in | ios::binary);

        CountID--;

    }
    prim.close();
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

void printPrimaryIndex(short CountID) {
    fstream prim(DOCTOR_INDEX_FILE, ios::in | ios::binary);

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
    insertInPrimaryIndex(const_cast<char*>(id.c_str()), (short)recOffset, DOCTOR_INDEX_FILE,DocCountID);



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
    if (slot != -1) writeActiveBodyAt(f, slot, body), cout << "Appointment added (reused deleted slot)\n";
    else appendRecord(f, body), cout << "Appointment appended at file end\n";
    f.close();
    /* ---- UPDATE PRIMARY INDEX FOR APPOINTMENT ---- */
    insertInPrimaryIndex(
        const_cast<char*>(appID.c_str()),(short)recOffset,APPOINTMENT_INDEX_FILE,appointmentCountID);

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

// -------------------- Menu --------------------
void menu() {
    ensureFile(DOCTOR_FILE);
    ensureFile(APPOINTMENT_FILE);
    ensureFile(DOCTOR_INDEX_FILE);
    DocCountID = loadCountID(DOCTOR_INDEX_FILE);
    appointmentCountID = loadCountID(APPOINTMENT_FILE);


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
