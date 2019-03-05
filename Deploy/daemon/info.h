
struct Student {
    int id;
    string name;
    bool verified;

    Student(int i, string k, bool v) {
        id = i;
        name = k;
        verified = v;
    }
};

string encode(Student input) {
    return input.name + "#" + to_string(input.id) + "#" + to_string(input.verified);
}

Student decode(string input) {
    size_t found = input.find_first_of("#");
    string name = input.substr(0, found);
    string rest = input.substr(found + 1);
    found = rest.find_first_of("#");
    int id = stoi(rest.substr(0, found));
    bool v = stoi(rest.substr(found + 1));
    return Student(id, name, v);
}

void prints(Student input) {
    cout << "id: " << input.id << " name: " << input.name << endl;
    if(input.verified) {
        cout << "The student is verified." << endl;
    } else {
        cout << "The student is not verified." << endl;
    }
    cout << endl;
}
