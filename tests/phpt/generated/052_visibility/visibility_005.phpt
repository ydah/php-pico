--TEST--
private and protected member access vector 005
--FILE--
<?php
class GeneratedVisibility005 {
    private $left = 39; protected $right = 31;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility005())->total();
--EXPECT--
70
