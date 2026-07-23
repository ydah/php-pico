--TEST--
magic get set and string conversion vector 002
--FILE--
<?php
class GeneratedMagic002 {
    private $value = 24;
    public function __get($name) { return $name . ':' . $this->value; }
    public function __set($name, $value) { $this->value = $value; }
    public function __toString() { return 'box=' . $this->value; }
}
$box = new GeneratedMagic002(); echo $box->missing, ':';
$box->dynamic = 27; echo $box;
--EXPECT--
missing:24:box=27
