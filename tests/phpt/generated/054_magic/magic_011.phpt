--TEST--
magic get set and string conversion vector 011
--FILE--
<?php
class GeneratedMagic011 {
    private $value = 123;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic011(); echo $box->missing, ':';
$box->dynamic = 126; echo $box;
--EXPECT--
missing:123:box=126
