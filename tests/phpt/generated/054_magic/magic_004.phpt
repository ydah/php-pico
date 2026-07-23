--TEST--
magic get set and string conversion vector 004
--FILE--
<?php
class GeneratedMagic004 {
    private $value = 46;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic004(); echo $box->missing, ':';
$box->dynamic = 49; echo $box;
--EXPECT--
missing:46:box=49
