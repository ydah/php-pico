--TEST--
magic get set and string conversion vector 012
--FILE--
<?php
class GeneratedMagic012 {
    private $value = 134;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic012(); echo $box->missing, ':';
$box->dynamic = 137; echo $box;
--EXPECT--
missing:134:box=137
