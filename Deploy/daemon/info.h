
struct Student {
    int id;
    string name;

    Student(int i, string k) {
        id = i;
        name = k;
    }
};

string encode(Student input) {
    return input.name + "#" + to_string(input.id);
}

Student decode(string input) {
    size_t found = input.find_first_of("#");
    string name = input.substr(0, found);
    int id = stoi(input.substr(found+1));
    return Student(id, name);
}

void prints(Student input) {
    cout << "id: " << input.id << " name: " << input.name << endl;
}
