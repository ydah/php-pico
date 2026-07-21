<?php

class Counter {
    private int $value = 0;

    public function __construct(int $start) {
        $this->value = $start;
    }

    public function increment(): int {
        $this->value = $this->value + 1;
        return $this->value;
    }
}

$counter = new Counter(40);
echo $counter->increment(), "\n";
echo $counter->increment(), "\n";
