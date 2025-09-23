#include <zephyr/kernel.h>
#include <cJSON.h>

void main(void)
{
    const char *json_text = "{\"name\":\"Amadeus\", \"age\":25, \"skills\":[\"C\",\"Rust\",\"Go\"]}";

    cJSON *root = cJSON_Parse(json_text);

    if (root == NULL) {
        printk("Error parsing JSON!\n");
        return;
    }

    cJSON *name = cJSON_GetObjectItem(root, "name");
    if (cJSON_IsString(name)) {
        printk("Name: %s\n", name->valuestring);
    }

    cJSON *age = cJSON_GetObjectItem(root, "age");
    if (cJSON_IsNumber(age)) {
        printk("Age: %d\n", age->valueint);
    }

    cJSON *skills = cJSON_GetObjectItem(root, "skills");
    if (cJSON_IsArray(skills)) {
        int size = cJSON_GetArraySize(skills);
        printk("Skills:\n");
        for (int i = 0; i < size; i++) {
            cJSON *item = cJSON_GetArrayItem(skills, i);
            if (cJSON_IsString(item)) {
                printk("  - %s\n", item->valuestring);
            }
        }
    }

    cJSON_Delete(root);
    return;
}
